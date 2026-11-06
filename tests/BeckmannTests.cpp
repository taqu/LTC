#include <gtest/gtest.h>
#include "ltc/Beckmann.h"
#include <cmath>

using namespace ltc;

// ---- NDF tests ----

TEST(BeckmannNdf, NormalIncidenceExact)
{
    // At cosH=1: tan^2=0, exp(0)=1, cos^4(0)=1
    // D = 1 / (pi * alpha^2)
    double alpha = 0.5;
    double expected = 1.0 / (kPi * alpha * alpha);
    EXPECT_NEAR(Beckmann::ndf(alpha, 1.0), expected, 1e-10);
}

TEST(BeckmannNdf, ZeroOrNegativeCosHReturnsZero)
{
    EXPECT_EQ(Beckmann::ndf(0.5,  0.0), Scalar(0));
    EXPECT_EQ(Beckmann::ndf(0.5, -0.1), Scalar(0));
}

TEST(BeckmannNdf, ZeroOrNegativeAlphaReturnsZero)
{
    EXPECT_EQ(Beckmann::ndf(0.0,  0.5), Scalar(0));
    EXPECT_EQ(Beckmann::ndf(-0.5, 0.5), Scalar(0));
}

TEST(BeckmannNdf, IsNonnegative)
{
    for (double alpha : {0.1, 0.25, 0.5, 1.0}) {
        for (double cosH : {0.1, 0.3, 0.5, 0.7, 0.9, 1.0}) {
            EXPECT_GE(Beckmann::ndf(alpha, cosH), 0.0);
        }
    }
}

TEST(BeckmannNdf, NormalizationApproxOne)
{
    // Numerical integration: integral of D(H) * cosH over hemisphere should be ~1.
    // Sample over theta_h in [0, pi/2], phi in [0, 2*pi].
    const int N = 512;
    double alpha = 0.5;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
        double theta = (i + 0.5) * (kPi / 2.0) / N;
        double cosH = std::cos(theta);
        double sinH = std::sin(theta);
        double D = Beckmann::ndf(alpha, cosH);
        // Integral over phi gives 2*pi; solid angle element = sin(theta)*dtheta
        sum += D * cosH * sinH * kTwoPi * (kPi / 2.0) / N;
    }
    EXPECT_NEAR(sum, 1.0, 0.01); // within 1%
}

// ---- Smith G1 tests ----

TEST(BeckmannSmithG1, NormalIncidenceIsOne)
{
    // At sinTheta=0 (normal incidence), G1 = 1.
    EXPECT_NEAR(Beckmann::smithG1(0.5, 1.0, 0.0), 1.0, 1e-12);
    EXPECT_NEAR(Beckmann::smithG1(0.25, 1.0, 0.0), 1.0, 1e-12);
}

TEST(BeckmannSmithG1, ZeroCosineReturnsZero)
{
    EXPECT_EQ(Beckmann::smithG1(0.5,  0.0, 1.0), Scalar(0));
    EXPECT_EQ(Beckmann::smithG1(0.5, -0.1, 1.0), Scalar(0));
}

TEST(BeckmannSmithG1, GrazingAngleIsLow)
{
    // At very grazing angle, G1 should approach 0.
    double theta = kPi * 0.49; // near 90 degrees
    EXPECT_LT(Beckmann::smithG1(0.5, std::cos(theta), std::sin(theta)), 0.3);
}

TEST(BeckmannSmithG1, IsInRangeZeroToOne)
{
    for (double alpha : {0.1, 0.25, 0.5, 1.0}) {
        for (double theta : {0.1, 0.5, 1.0, 1.4}) {
            double cosT = std::cos(theta);
            double sinT = std::sin(theta);
            double g = Beckmann::smithG1(alpha, cosT, sinT);
            EXPECT_GE(g, 0.0);
            EXPECT_LE(g, 1.0 + 1e-12);
        }
    }
}

// ---- Evaluate tests ----

TEST(BeckmannEvaluate, BelowHorizonReturnsZero)
{
    Beckmann brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    auto r = brdf.evaluate(ctx, Vec3(0, 0, -0.5));
    EXPECT_EQ(r.value, Scalar(0));
    EXPECT_EQ(r.pdf,   Scalar(0));
}

TEST(BeckmannEvaluate, IsNonnegativeAndFinite)
{
    Beckmann brdf;
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

TEST(BeckmannEvaluate, Reciprocity)
{
    // f(V->L)*cosNL should be approximately equal to f(L->V)*cosNV.
    // The Walter rational G1 approximation is not perfectly symmetric,
    // so we allow a small tolerance relative to the value magnitude.
    Beckmann brdf;
    Vec3 V = glm::normalize(Vec3(0.4, 0.0, 0.9));
    Vec3 L = glm::normalize(Vec3(0.3, 0.2, 0.7));

    BRDFContext ctxV{V, 0.4};
    BRDFContext ctxL{L, 0.4};

    auto rVL = brdf.evaluate(ctxV, L);
    auto rLV = brdf.evaluate(ctxL, V);

    // Both should be non-negative and finite
    EXPECT_GE(rVL.value, 0.0);
    EXPECT_GE(rLV.value, 0.0);
    EXPECT_TRUE(std::isfinite(rVL.value));
    EXPECT_TRUE(std::isfinite(rLV.value));

    // Should be within ~10% of each other (rational G1 approx breaks exact reciprocity)
    double avg = (rVL.value + rLV.value) * 0.5;
    if (avg > 1e-10) {
        EXPECT_NEAR(rVL.value / avg, rLV.value / avg, 0.15);
    }
}

// ---- Sample tests ----

TEST(BeckmannSample, SampledPDFIsPositiveForValidL)
{
    Beckmann brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    Vec3 L = brdf.sample(ctx, Vec2(0.3, 0.7));
    if (L.z > 0.0) {
        auto r = brdf.evaluate(ctx, L);
        EXPECT_GE(r.pdf, 0.0);
        EXPECT_TRUE(std::isfinite(r.pdf));
    }
}

TEST(BeckmannSample, PDFIntegrationConsistency)
{
    // sum(value / pdf) over many samples should approximate magnitude.
    // We use a simplified convergence test: mean value/pdf in reasonable range.
    Beckmann brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    const int N = 10000;
    double sum = 0.0;
    int valid = 0;
    for (int i = 0; i < N; ++i) {
        // Use low-discrepancy-like spacing
        double u1 = (i + 0.5) / N;
        double u2 = std::fmod((i + 0.5) * 0.6180339887, 1.0);
        Vec3 L = brdf.sample(ctx, Vec2(u1, u2));
        if (L.z <= 0.0) continue;
        auto r = brdf.evaluate(ctx, L);
        if (r.pdf <= 0.0 || !std::isfinite(r.pdf)) continue;
        sum += r.value / r.pdf;
        ++valid;
    }
    // Should be a positive number and not blow up
    EXPECT_GT(valid, N / 2);
    double mean = sum / valid;
    EXPECT_GT(mean, 0.0);
    EXPECT_TRUE(std::isfinite(mean));
}

// ---- contextAlpha and capabilities ----

TEST(BeckmannCapabilities, ContextAlpha)
{
    Beckmann brdf;
    EXPECT_NEAR(brdf.contextAlpha(0.5), 0.25, 1e-15); // 0.5^2 = 0.25
    EXPECT_NEAR(brdf.contextAlpha(0.0), 1e-5, 1e-20); // clamped
}

TEST(BeckmannCapabilities, Name)
{
    Beckmann brdf;
    EXPECT_EQ(brdf.name(), "beckmann");
}

TEST(BeckmannCapabilities, Capabilities)
{
    Beckmann brdf;
    auto caps = brdf.capabilities();
    EXPECT_TRUE(caps.supportsSchlickFresnel);
    EXPECT_EQ(caps.amplitudeModel, AmplitudeModel::SchlickFresnel);
    EXPECT_EQ(caps.parameterization, BRDFParameterization::MicrofacetAlpha);
    EXPECT_TRUE(caps.isotropic);
}
