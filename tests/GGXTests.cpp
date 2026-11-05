#include <gtest/gtest.h>
#include "ltc/GGX.h"
#include <cmath>

using namespace ltc;

// ---- NDF tests ----

TEST(GGXNdf, AlphaOneIsUniformOverHemisphere)
{
    // At alpha=1: D = 1/pi for any cosH in (0,1].
    // Because (cosH^2*(1-1) + 1) = 1, so D = 1 / pi.
    constexpr Scalar expected = Scalar(1) / kPi;
    EXPECT_NEAR(GGX::ndf(1.0, 1.0),  expected, 1e-14);
    EXPECT_NEAR(GGX::ndf(1.0, 0.7),  expected, 1e-14);
    EXPECT_NEAR(GGX::ndf(1.0, 0.1),  expected, 1e-14);
}

TEST(GGXNdf, NormalIncidenceExact)
{
    // At cosH=1: D = 1 / (pi * alpha^2).
    EXPECT_NEAR(GGX::ndf(0.25, 1.0), 1.0 / (kPi * 0.25 * 0.25), 1e-12);
    EXPECT_NEAR(GGX::ndf(0.5,  1.0), 1.0 / (kPi * 0.5  * 0.5 ), 1e-12);
    EXPECT_NEAR(GGX::ndf(1.0,  1.0), 1.0 / kPi,                  1e-12);
}

TEST(GGXNdf, ZeroOrNegativeAngleReturnsZero)
{
    EXPECT_EQ(GGX::ndf(0.5,  0.0), Scalar(0));
    EXPECT_EQ(GGX::ndf(0.5, -0.1), Scalar(0));
}

TEST(GGXNdf, ZeroOrNegativeAlphaReturnsZero)
{
    EXPECT_EQ(GGX::ndf(0.0, 0.5), Scalar(0));
    EXPECT_EQ(GGX::ndf(-1.0, 0.5), Scalar(0));
}

TEST(GGXNdf, IsNonnegative)
{
    for (double alpha : {0.01, 0.1, 0.25, 0.5, 1.0}) {
        for (double cosH : {0.1, 0.3, 0.5, 0.7, 0.9, 1.0}) {
            EXPECT_GE(GGX::ndf(alpha, cosH), 0.0);
        }
    }
}

TEST(GGXNdf, SmallRoughnessHasHighPeakAtNormal)
{
    // As alpha → 0 the NDF concentrates at cosH = 1.
    EXPECT_GT(GGX::ndf(0.01, 1.0), GGX::ndf(0.5, 1.0));
    EXPECT_GT(GGX::ndf(0.01, 1.0), 1e3);
}

// ---- Smith G1 tests ----

TEST(GGXSmithG1, PerfectlySmoothSurface)
{
    // alpha=0: G1 = 2*cosV / (cosV + cosV) = 1.
    // Our implementation clamps alpha to kMinAlpha but we test the formula.
    // For the formula: sqrt(0 + cosV^2) = cosV, so G1 = 2*cosV/(2*cosV) = 1.
    // smithG1 is called with alpha=0 directly (kMinAlpha clamping is in GGX::evaluate).
    EXPECT_NEAR(GGX::smithG1(0.0, 0.5),  1.0, 1e-14);
    EXPECT_NEAR(GGX::smithG1(0.0, 1.0),  1.0, 1e-14);
}

TEST(GGXSmithG1, NormalIncidenceIsOne)
{
    // At cosV=1: G1 = 2/(1 + sqrt(alpha^2 + (1-alpha^2))) = 2/(1+1) = 1.
    EXPECT_NEAR(GGX::smithG1(0.25, 1.0), 1.0, 1e-14);
    EXPECT_NEAR(GGX::smithG1(0.5,  1.0), 1.0, 1e-14);
    EXPECT_NEAR(GGX::smithG1(1.0,  1.0), 1.0, 1e-14);
}

TEST(GGXSmithG1, ZeroCosineReturnsZero)
{
    EXPECT_EQ(GGX::smithG1(0.5, 0.0),  Scalar(0));
    EXPECT_EQ(GGX::smithG1(0.5, -0.1), Scalar(0));
}

TEST(GGXSmithG1, GrazingAngleIsLow)
{
    // At cosV → 0, G1 → 0.
    EXPECT_LT(GGX::smithG1(0.5, 0.01), 0.1);
}

TEST(GGXSmithG1, IsInRangeZeroToOne)
{
    for (double alpha : {0.1, 0.25, 0.5, 1.0}) {
        for (double cosV : {0.1, 0.3, 0.5, 0.7, 0.9, 1.0}) {
            Scalar g = GGX::smithG1(alpha, cosV);
            EXPECT_GE(g, 0.0);
            EXPECT_LE(g, 1.0 + 1e-12);
        }
    }
}

// ---- Evaluate / PDF tests ----

TEST(GGXEvaluate, BelowHorizonReturnsZero)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    auto r = brdf.evaluate(ctx, Vec3(0, 0, -0.5));
    EXPECT_EQ(r.value, Scalar(0));
    EXPECT_EQ(r.pdf, Scalar(0));
}

TEST(GGXEvaluate, IsNonnegativeAndFinite)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    for (double x : {0.0, 0.3, 0.6}) {
        for (double y : {0.0, 0.3}) {
            double z = std::sqrt(std::max(0.0, 1.0 - x*x - y*y));
            if (z <= 0.0) continue;
            Vec3 L = glm::normalize(Vec3(x, y, z));
            auto r = brdf.evaluate(ctx, L);
            EXPECT_GE(r.value, 0.0);
            EXPECT_GE(r.pdf,   0.0);
            EXPECT_TRUE(std::isfinite(r.value));
            EXPECT_TRUE(std::isfinite(r.pdf));
        }
    }
}

TEST(GGXEvaluate, IsotropicRotationalInvariance)
{
    // Rotating L around N by an angle should not change value or pdf for
    // isotropic GGX (view V is in the XZ plane, rotation is around Z).
    GGX brdf;
    Vec3 V = glm::normalize(Vec3(0.6, 0.0, 0.8));
    BRDFContext ctx{V, 0.3};

    // L and R(phi)*L should give the same (value, pdf) only when V is at the
    // same phi.  Instead we test: for V in XZ plane, mirroring L across XZ
    // gives the same result (L.y → -L.y).
    Vec3 L  = glm::normalize(Vec3(0.4, 0.3, 0.7));
    Vec3 Lm = glm::normalize(Vec3(0.4, -0.3, 0.7));

    auto r1 = brdf.evaluate(ctx, L);
    auto r2 = brdf.evaluate(ctx, Lm);

    EXPECT_NEAR(r1.value, r2.value, 1e-10);
    EXPECT_NEAR(r1.pdf,   r2.pdf,   1e-10);
}

TEST(GGXSample, DeterministicSampling)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    Vec2 u(0.3, 0.7);
    Vec3 d1 = brdf.sample(ctx, u);
    Vec3 d2 = brdf.sample(ctx, u);
    EXPECT_DOUBLE_EQ(d1.x, d2.x);
    EXPECT_DOUBLE_EQ(d1.y, d2.y);
    EXPECT_DOUBLE_EQ(d1.z, d2.z);
}

TEST(GGXSample, SampledPDFIsConsistentWithEvaluate)
{
    // The PDF returned by evaluate should equal GGX::pdf for the same (V,L).
    GGX brdf;
    Vec3 V = glm::normalize(Vec3(0.4, 0.0, 0.9));
    BRDFContext ctx{V, 0.3};
    Vec2 u(0.4, 0.6);
    Vec3 L = brdf.sample(ctx, u);
    if (L.z <= 0.0) return; // skip below-horizon sample

    auto r   = brdf.evaluate(ctx, L);
    Scalar p = GGX::pdf(ctx.alpha, ctx.view, L);

    EXPECT_NEAR(r.pdf, p, 1e-10);
}

TEST(GGXPdf, BelowHorizonReturnsZero)
{
    Vec3 V = glm::normalize(Vec3(0.0, 0.0, 1.0));
    EXPECT_EQ(GGX::pdf(0.5, V, Vec3(0, 0, -1)), Scalar(0));
}
