#include <gtest/gtest.h>
#include "ltc/LTC.h"
#include <cmath>

using namespace ltc;

// ---- Default / identity state ----

TEST(LTCDefault, MatrixIsIdentity)
{
    LTC ltc;
    // Default parameters: m11=1, m22=1, m13=0, identity basis.
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row) {
            Scalar expected = (col == row) ? Scalar(1) : Scalar(0);
            EXPECT_NEAR(ltc.matrix()[col][row], expected, 1e-14);
        }
}

TEST(LTCDefault, InverseMatrixIsIdentity)
{
    LTC ltc;
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row) {
            Scalar expected = (col == row) ? Scalar(1) : Scalar(0);
            EXPECT_NEAR(ltc.inverseMatrix()[col][row], expected, 1e-14);
        }
}

TEST(LTCDefault, MagnitudeIsOne)
{
    LTC ltc;
    EXPECT_DOUBLE_EQ(ltc.magnitude(), 1.0);
}

// ---- Identity LTC matches clamped cosine ----

TEST(LTCIdentity, PDFMatchesClampedCosine)
{
    LTC ltc; // identity matrix, magnitude=1

    // D_LTC(w; I) == D_cos(w) == max(0, w.z) / pi
    struct { Vec3 dir; Scalar expectedPdf; } cases[] = {
        { Vec3(0, 0, 1),                                   Scalar(1) / kPi },
        { glm::normalize(Vec3(0.6, 0, 0.8)),               Scalar(0.8)     / kPi },
        { glm::normalize(Vec3(0.5, 0.5, std::sqrt(0.5))), std::sqrt(0.5)  / kPi },
        { Vec3(1, 0, 0),                                   Scalar(0)                },
        { Vec3(0, 0, -1),                                  Scalar(0)                },
    };

    for (auto& c : cases) {
        EXPECT_NEAR(ltc.pdf(c.dir), c.expectedPdf, 1e-12)
            << "direction: " << c.dir.x << " " << c.dir.y << " " << c.dir.z;
    }
}

TEST(LTCIdentity, EvaluateEqualsPDFWhenMagnitudeIsOne)
{
    LTC ltc;
    Vec3 dir = glm::normalize(Vec3(0.3, 0.4, 0.8));
    EXPECT_NEAR(ltc.evaluate(dir), ltc.pdf(dir), 1e-14);
}

// ---- setParameters ----

TEST(LTCParameters, SetValidParameters)
{
    LTC ltc;
    EXPECT_TRUE(ltc.setParameters(0.8, 1.2, 0.15));
    EXPECT_DOUBLE_EQ(ltc.m11(), 0.8);
    EXPECT_DOUBLE_EQ(ltc.m22(), 1.2);
    EXPECT_DOUBLE_EQ(ltc.m13(), 0.15);
}

TEST(LTCParameters, MatrixInverseConsistency)
{
    LTC ltc;
    ltc.setParameters(0.8, 1.2, 0.15);

    // M * Minv should be (close to) identity.
    Mat3 product = ltc.matrix() * ltc.inverseMatrix();
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row) {
            Scalar expected = (col == row) ? Scalar(1) : Scalar(0);
            EXPECT_NEAR(product[col][row], expected, 1e-10);
        }
}

TEST(LTCParameters, AbsDetInverseConsistency)
{
    LTC ltc;
    ltc.setParameters(0.8, 1.2, 0.15);

    // |det(Minv)| should equal 1 / |det(M)|.
    Scalar detM    = ltc::determinant(ltc.matrix());
    Scalar expected = Scalar(1) / std::abs(detM);
    EXPECT_NEAR(ltc.absDetInverse(), expected, 1e-12);
}

// ---- Magnitude independence ----

TEST(LTCMagnitude, PDFUnchangedByMagnitude)
{
    LTC ltc;
    Vec3 dir = glm::normalize(Vec3(0.3, 0.0, 0.8));

    Scalar pdf1 = ltc.pdf(dir);
    ltc.setMagnitude(3.5);
    Scalar pdf2 = ltc.pdf(dir);

    EXPECT_DOUBLE_EQ(pdf1, pdf2);
}

TEST(LTCMagnitude, EvaluateScalesWithMagnitude)
{
    LTC ltc;
    Vec3 dir = glm::normalize(Vec3(0.3, 0.0, 0.8));

    Scalar eval1 = ltc.evaluate(dir);
    ltc.setMagnitude(2.0);
    Scalar eval2 = ltc.evaluate(dir);

    EXPECT_NEAR(eval2, 2.0 * eval1, 1e-14);
}

TEST(LTCMagnitude, SampledDirectionsUnchanged)
{
    LTC ltc;
    Vec2 u(0.3, 0.7);

    Vec3 d1 = ltc.sample(u);
    ltc.setMagnitude(99.0);
    Vec3 d2 = ltc.sample(u);

    EXPECT_DOUBLE_EQ(d1.x, d2.x);
    EXPECT_DOUBLE_EQ(d1.y, d2.y);
    EXPECT_DOUBLE_EQ(d1.z, d2.z);
}

// ---- Sampling ----

TEST(LTCSampling, ReturnedDirectionIsUnit)
{
    LTC ltc;
    ltc.setParameters(0.8, 1.2, 0.15);
    for (double u1 : {0.1, 0.3, 0.5, 0.7, 0.9}) {
        for (double u2 : {0.1, 0.5, 0.9}) {
            Vec3 dir = ltc.sample(Vec2(u1, u2));
            EXPECT_NEAR(glm::length(dir), 1.0, 1e-12);
        }
    }
}

TEST(LTCSampling, DeterministicSampling)
{
    LTC ltc;
    Vec2 u(0.4, 0.6);
    Vec3 d1 = ltc.sample(u);
    Vec3 d2 = ltc.sample(u);
    EXPECT_DOUBLE_EQ(d1.x, d2.x);
    EXPECT_DOUBLE_EQ(d1.y, d2.y);
    EXPECT_DOUBLE_EQ(d1.z, d2.z);
}

// ---- Invalid inputs ----

TEST(LTCInvalid, ZeroDirectionReturnsPDFZero)
{
    LTC ltc;
    // The zero vector should not crash — returns 0.
    Scalar p = ltc.pdf(Vec3(0, 0, 0));
    EXPECT_EQ(p, Scalar(0));
}

TEST(LTCSetBasis, RejectsNonOrthonormalBasis)
{
    LTC ltc;
    // Non-unit vectors
    EXPECT_FALSE(ltc.setBasis(Vec3(2, 0, 0), Vec3(0, 1, 0), Vec3(0, 0, 1)));
    // Non-orthogonal
    EXPECT_FALSE(ltc.setBasis(Vec3(1, 0, 0), Vec3(0.1, 1, 0), Vec3(0, 0, 1)));
}

TEST(LTCSetBasis, AcceptsValidBasis)
{
    LTC ltc;
    // 90-degree rotation around Z
    EXPECT_TRUE(ltc.setBasis(Vec3(0, 1, 0), Vec3(-1, 0, 0), Vec3(0, 0, 1)));
}
