#include <gtest/gtest.h>
#include "ltc/Sampling.h"
#include <cmath>

using namespace ltc;

TEST(CosineHemisphere, PDFAtZenithIsInvPi)
{
    Vec3 zenith(0, 0, 1);
    EXPECT_NEAR(cosineHemispherePdf(zenith), kInvPi, 1e-14);
}

TEST(CosineHemisphere, PDFBelowHorizonIsZero)
{
    EXPECT_EQ(cosineHemispherePdf(Vec3(0, 0, -1)), Scalar(0));
    EXPECT_EQ(cosineHemispherePdf(Vec3(1, 0, 0)),  Scalar(0));
}

TEST(CosineHemisphere, SampleReturnedDirectionIsUnit)
{
    for (double u1 : {0.0, 0.1, 0.5, 0.9, 0.99}) {
        for (double u2 : {0.0, 0.25, 0.5, 0.75}) {
            auto s = sampleCosineHemisphere(Vec2(u1, u2));
            EXPECT_NEAR(glm::length(s.direction), 1.0, 1e-12);
            EXPECT_GE(s.direction.z, 0.0);
        }
    }
}

TEST(CosineHemisphere, SamplePDFMatchesEvaluated)
{
    for (double u1 : {0.1, 0.3, 0.5, 0.7, 0.9}) {
        for (double u2 : {0.1, 0.4, 0.8}) {
            auto s = sampleCosineHemisphere(Vec2(u1, u2));
            Scalar evaluated = cosineHemispherePdf(s.direction);
            EXPECT_NEAR(s.pdf, evaluated, 1e-12);
        }
    }
}

TEST(CosineHemisphere, PDFEqualsCosThetaOverPi)
{
    // pdf = cos(theta)/pi = direction.z / pi
    for (double u1 : {0.1, 0.4, 0.8}) {
        auto s = sampleCosineHemisphere(Vec2(u1, 0.5));
        Scalar expected = s.direction.z * kInvPi;
        EXPECT_NEAR(s.pdf, expected, 1e-12);
    }
}

TEST(CosineHemisphere, DeterministicSampling)
{
    Vec2 u(0.3, 0.6);
    auto s1 = sampleCosineHemisphere(u);
    auto s2 = sampleCosineHemisphere(u);
    EXPECT_DOUBLE_EQ(s1.direction.x, s2.direction.x);
    EXPECT_DOUBLE_EQ(s1.direction.y, s2.direction.y);
    EXPECT_DOUBLE_EQ(s1.direction.z, s2.direction.z);
    EXPECT_DOUBLE_EQ(s1.pdf,         s2.pdf);
}
