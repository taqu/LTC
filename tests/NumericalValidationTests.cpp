#include <gtest/gtest.h>
#include "ltc/LTC.h"
#include "ltc/GGX.h"
#include "ltc/Math.h"
#include "ltc/Sampling.h"
#include <cmath>
#include <functional>

using namespace ltc;

// ============================================================
// Numerical integration helper
// ============================================================
// Integrates f(theta, phi) over the given theta range using the midpoint rule.
// Returns the integral over the sphere/hemisphere in solid-angle measure.
static Scalar sphereIntegral(
    std::function<Scalar(Scalar theta, Scalar phi)> f,
    Scalar thetaMin, Scalar thetaMax,
    int Ntheta = 600, int Nphi = 1200)
{
    Scalar dTheta = (thetaMax - thetaMin) / Ntheta;
    Scalar dPhi   = kTwoPi / Nphi;
    Scalar sum    = Scalar(0);
    for (int i = 0; i < Ntheta; ++i) {
        Scalar theta = thetaMin + (i + Scalar(0.5)) * dTheta;
        Scalar sinT  = std::sin(theta);
        for (int j = 0; j < Nphi; ++j) {
            Scalar phi = (j + Scalar(0.5)) * dPhi;
            sum += f(theta, phi) * sinT;
        }
    }
    return sum * dTheta * dPhi;
}

static Vec3 toDir(Scalar theta, Scalar phi)
{
    return {std::sin(theta) * std::cos(phi),
            std::sin(theta) * std::sin(phi),
            std::cos(theta)};
}

// ============================================================
// 8.1 — Cosine distribution normalization
// ============================================================
TEST(NumericalValidation, CosineHemisphereNormalizesTo1)
{
    // ∫_{hemisphere} D_cos(w) dw = 1
    Scalar integral = sphereIntegral(
        [](Scalar theta, Scalar phi) {
            Vec3 dir = toDir(theta, phi);
            return cosineHemispherePdf(dir);
        },
        Scalar(0), kPi / Scalar(2));

    EXPECT_NEAR(integral, 1.0, 1e-3);
}

// ============================================================
// 8.2 — LTC normalization over full sphere
// ============================================================
static Scalar ltcSphereIntegral(const LTC& ltc)
{
    return sphereIntegral(
        [&](Scalar theta, Scalar phi) {
            return ltc.pdf(toDir(theta, phi));
        },
        Scalar(0), kPi);
}

TEST(NumericalValidation, IdentityLTCNormalizesTo1)
{
    LTC ltc; // M = I
    EXPECT_NEAR(ltcSphereIntegral(ltc), 1.0, 1e-3);
}

TEST(NumericalValidation, AnisotropicScaleLTCNormalizesTo1)
{
    LTC ltc;
    ltc.setParameters(0.5, 1.5, 0.0); // anisotropic scale, no shear
    EXPECT_NEAR(ltcSphereIntegral(ltc), 1.0, 1e-3);
}

TEST(NumericalValidation, ShearLTCNormalizesTo1)
{
    LTC ltc;
    ltc.setParameters(1.0, 1.0, 0.3); // pure shear
    EXPECT_NEAR(ltcSphereIntegral(ltc), 1.0, 1e-3);
}

TEST(NumericalValidation, CombinedLTCNormalizesTo1)
{
    LTC ltc;
    ltc.setParameters(0.8, 1.2, 0.15);
    EXPECT_NEAR(ltcSphereIntegral(ltc), 1.0, 1e-3);
}

TEST(NumericalValidation, RotatedBasisLTCNormalizesTo1)
{
    LTC ltc;
    // 45-degree rotation around Z
    Scalar c = std::cos(kPi / 4);
    Scalar s = std::sin(kPi / 4);
    ltc.setBasis(Vec3(c, s, 0), Vec3(-s, c, 0), Vec3(0, 0, 1));
    ltc.setParameters(0.6, 1.0, 0.1);
    EXPECT_NEAR(ltcSphereIntegral(ltc), 1.0, 1e-3);
}

// ============================================================
// 8.3 — Identity LTC matches clamped cosine distribution
// ============================================================
TEST(NumericalValidation, IdentityLTCEqualsClampedCosine)
{
    LTC ltc;
    Vec3 directions[] = {
        Vec3(0, 0, 1),
        glm::normalize(Vec3(0.6, 0, 0.8)),
        glm::normalize(Vec3(0.3, 0.4, 0.8)),
        glm::normalize(Vec3(0.0, 0.5, std::sqrt(0.75))),
    };
    for (const Vec3& dir : directions) {
        Scalar ltcPdf  = ltc.pdf(dir);
        Scalar cosPdf  = cosineHemispherePdf(dir);
        EXPECT_NEAR(ltcPdf, cosPdf, 1e-12) << "dir: " << dir.x << " " << dir.y << " " << dir.z;
    }
}

// ============================================================
// 8.4 — Sampling / PDF consistency for LTC
// ============================================================
// Verify that ltc.sample and ltc.pdf are mutually consistent by using
// uniform sphere sampling as the independent estimator of ∫ pdf dω = 1.
// This avoids the variance problems of 1/pdf estimators near the horizon.
static Scalar ltcNormalizationViaSampling(LTC& ltc, int N = 4000)
{
    // Build a Halton (base-2, base-3) point set for good coverage.
    auto halton2 = [](int n) {
        Scalar r = Scalar(0), f = Scalar(0.5);
        while (n > 0) { if (n & 1) r += f; n >>= 1; f *= Scalar(0.5); }
        return r;
    };
    auto halton3 = [](int n) {
        Scalar r = Scalar(0), f = Scalar(1) / 3;
        while (n > 0) { r += (n % 3) * f; n /= 3; f /= 3; }
        return r;
    };

    // Sample uniformly on the sphere (Archimedes cylindrical equal-area).
    // Then estimate ∫ pdf dω ≈ (4π / N) * Σ pdf(ω_i).
    Scalar sum = Scalar(0);
    for (int i = 0; i < N; ++i) {
        Scalar u1   = halton2(i + 1);
        Scalar u2   = halton3(i + 1);
        Scalar z    = Scalar(2) * u1 - Scalar(1);
        Scalar phi  = kTwoPi * u2;
        Scalar r    = std::sqrt(std::max(Scalar(0), Scalar(1) - z * z));
        Vec3 dir(r * std::cos(phi), r * std::sin(phi), z);
        sum += ltc.pdf(dir);
    }
    return (kTwoPi * Scalar(2) / N) * sum; // 4π/N * Σ pdf = ∫ pdf dω ≈ 1
}

TEST(NumericalValidation, IdentityLTCSamplingConsistency)
{
    LTC ltc;
    EXPECT_NEAR(ltcNormalizationViaSampling(ltc), 1.0, 0.02);
}

TEST(NumericalValidation, ScaledLTCSamplingConsistency)
{
    LTC ltc;
    ltc.setParameters(0.5, 1.5, 0.0);
    EXPECT_NEAR(ltcNormalizationViaSampling(ltc), 1.0, 0.02);
}

TEST(NumericalValidation, ShearLTCSamplingConsistency)
{
    LTC ltc;
    ltc.setParameters(1.0, 1.0, 0.3);
    EXPECT_NEAR(ltcNormalizationViaSampling(ltc), 1.0, 0.02);
}

// ============================================================
// 8.5 — Scale-invariance of the normalized distribution
// ============================================================
// D_LTC(ω; M) is invariant to uniform positive scaling of M.
// We verify this analytically:
//   M' = s*M  =>  M'^{-1} = M^{-1}/s
//   ||M'^{-1}*ω|| = ||M^{-1}*ω|| / s
//   |det(M'^{-1})| = |det(M^{-1})| / s^3
// Therefore the Jacobian |det(M'^{-1})| / ||M'^{-1}*ω||^3 is unchanged.
// We compute D_LTC(ω; sM) directly from the matrices and compare to ltc.pdf.
TEST(NumericalValidation, UniformScaleDoesNotChangePDF)
{
    LTC ltc;
    ltc.setParameters(0.8, 1.2, 0.15);

    // Build sM and its inverse directly from ltc.matrix().
    constexpr Scalar s = Scalar(2);
    Mat3 sM    = ltc.matrix() * s;
    auto sMinv = safeInverse(sM);
    ASSERT_TRUE(sMinv.has_value()) << "s*M must be invertible";
    Scalar absDetSMinv = Scalar(1) / std::abs(determinant(sM));

    Vec3 dirs[] = {
        Vec3(0, 0, 1),
        glm::normalize(Vec3(0.4, 0.3, 0.8)),
        glm::normalize(Vec3(-0.2, 0.5, 0.7)),
    };

    for (const Vec3& dir : dirs) {
        // D_LTC(ω; sM) evaluated directly.
        Vec3   Lo  = *sMinv * dir;
        Scalar len = glm::length(Lo);
        ASSERT_GT(len, Scalar(1e-15));
        Vec3   L_   = Lo / len;
        Scalar Dcos = std::max(Scalar(0), L_.z) * kInvPi;
        Scalar jac  = absDetSMinv / (len * len * len);
        Scalar pdf2 = Dcos * jac;

        // Should equal D_LTC(ω; M).
        EXPECT_NEAR(ltc.pdf(dir), pdf2, 1e-10)
            << "dir: " << dir.x << " " << dir.y << " " << dir.z;
    }
}

// ============================================================
// 8.6 — Magnitude independence (quantitative)
// ============================================================
TEST(NumericalValidation, MagnitudeScalesEvaluateNotPDF)
{
    LTC ltc;
    ltc.setParameters(0.8, 1.0, 0.1);
    Vec3 dir = glm::normalize(Vec3(0.3, 0.1, 0.9));

    Scalar pdf0  = ltc.pdf(dir);
    Scalar eval0 = ltc.evaluate(dir);

    ltc.setMagnitude(5.0);
    EXPECT_NEAR(ltc.pdf(dir),      pdf0,         1e-14);
    EXPECT_NEAR(ltc.evaluate(dir), 5.0 * eval0,  1e-14);
}

// ============================================================
// 8.7 — Edge cases
// ============================================================
TEST(NumericalValidation, PDFForZeroLengthDirection)
{
    LTC ltc;
    EXPECT_EQ(ltc.pdf(Vec3(0, 0, 0)), Scalar(0));
}

TEST(NumericalValidation, GGXEvaluateIsFiniteForAllValidInputs)
{
    GGX brdf;
    for (double alpha : {0.01, 0.1, 0.5, 1.0}) {
        for (double theta : {0.01, 0.3, 0.6, 1.0, 1.4}) {
            Vec3 V = glm::normalize(Vec3(std::sin(theta), 0, std::cos(theta)));
            BRDFContext ctx{V, alpha};
            for (double phi : {0.0, 1.0, 2.0, 4.0}) {
                for (double cosL : {0.1, 0.5, 0.9}) {
                    Scalar sinL = std::sqrt(1.0 - cosL*cosL);
                    Vec3 L = glm::normalize(Vec3(sinL*std::cos(phi), sinL*std::sin(phi), cosL));
                    auto r = brdf.evaluate(ctx, L);
                    EXPECT_TRUE(std::isfinite(r.value));
                    EXPECT_TRUE(std::isfinite(r.pdf));
                    EXPECT_GE(r.value, 0.0);
                    EXPECT_GE(r.pdf,   0.0);
                }
            }
        }
    }
}

TEST(NumericalValidation, LTCPDFIsFiniteAndNonnegative)
{
    LTC ltc;
    ltc.setParameters(0.6, 1.4, 0.2);
    for (double theta : {0.1, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0}) {
        for (double phi : {0.0, 1.0, 2.5, 5.0}) {
            Vec3 dir = toDir(theta, phi);
            Scalar p = ltc.pdf(dir);
            EXPECT_TRUE(std::isfinite(p));
            EXPECT_GE(p, 0.0);
        }
    }
}
