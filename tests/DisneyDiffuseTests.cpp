#include <gtest/gtest.h>
#include "ltc/DisneyDiffuse.h"
#include <cmath>

using namespace ltc;

TEST(DisneyDiffuseEvaluate, NormalIncidenceExact)
{
    // V=(0,0,1), L=(0,0,1), r=0.5
    // H = (0,0,1), cosLH = 1
    // FD90 = 0.5 + 2*0.5*1*1 = 1.5
    // FL = (1-1)^5 = 0, FV = 0
    // fd = (1/pi) * (1 + (1.5-1)*0) * (1 + (1.5-1)*0) = 1/pi
    // value = fd * cosNL = 1/pi * 1 = 1/pi
    DisneyDiffuse brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    auto r = brdf.evaluate(ctx, Vec3(0, 0, 1));
    EXPECT_NEAR(r.value, kInvPi, 1e-6);
}

TEST(DisneyDiffuseEvaluate, BelowHorizonReturnsZero)
{
    DisneyDiffuse brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    auto r = brdf.evaluate(ctx, Vec3(0, 0, -1));
    EXPECT_EQ(r.value, Scalar(0));
}

TEST(DisneyDiffuseEvaluate, Nonnegative)
{
    DisneyDiffuse brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    for (double x : {0.0, 0.3, 0.5}) {
        for (double y : {0.0, 0.3}) {
            double z = std::sqrt(std::max(0.0, 1.0 - x*x - y*y));
            if (z <= 0.0) continue;
            Vec3 L = glm::normalize(Vec3(x, y, z));
            auto r = brdf.evaluate(ctx, L);
            EXPECT_GE(r.value, 0.0);
            EXPECT_GE(r.pdf,   0.0);
        }
    }
}

TEST(DisneyDiffuseEvaluate, Reciprocity)
{
    // Disney diffuse is reciprocal: f(V,L) = f(L,V)
    // value = f*cosNL so we need to divide out cosNL to compare f values.
    DisneyDiffuse brdf;
    Vec3 V = glm::normalize(Vec3(0.3, 0.0, 0.8));
    Vec3 L = glm::normalize(Vec3(0.4, 0.2, 0.6));
    BRDFContext ctxV{V, 0.4};
    BRDFContext ctxL{L, 0.4};
    auto rVL = brdf.evaluate(ctxV, L);
    auto rLV = brdf.evaluate(ctxL, V);

    // rVL.value = f(V,L)*cosNL, rLV.value = f(L,V)*cosNV
    // Reciprocity: f(V,L) = f(L,V) => rVL.value/cosNL = rLV.value/cosNV
    double cosNL = L.z;
    double cosNV = V.z;
    if (cosNL > 1e-6 && cosNV > 1e-6) {
        double fVL = rVL.value / cosNL;
        double fLV = rLV.value / cosNV;
        EXPECT_NEAR(fVL, fLV, 1e-10);
    }
}

TEST(DisneyDiffuseEvaluate, PDFIsCosineWeighted)
{
    DisneyDiffuse brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.3};
    Vec3 L = glm::normalize(Vec3(0.3, 0.2, 0.9));
    auto r = brdf.evaluate(ctx, L);
    double expectedPdf = L.z * kInvPi;
    EXPECT_NEAR(r.pdf, expectedPdf, 1e-10);
}

TEST(DisneyDiffuseEvaluate, LambertianAtZeroRoughness)
{
    // At r=0, FD90 = 0.5 + 0 = 0.5
    // fd = (1/pi) * (1 + (0.5-1)*FL) * (1 + (0.5-1)*FV)
    // At normal incidence: FL=FV=0, so fd = 1/pi
    DisneyDiffuse brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.0};
    auto r = brdf.evaluate(ctx, Vec3(0, 0, 1));
    EXPECT_NEAR(r.value, kInvPi, 1e-10);
}

// ---- Sample tests ----

TEST(DisneyDiffuseSample, DirectionAboveHorizon)
{
    DisneyDiffuse brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    for (int i = 0; i < 20; ++i) {
        double u1 = (i + 0.5) / 20.0;
        double u2 = std::fmod(u1 * 1.618033, 1.0);
        Vec3 L = brdf.sample(ctx, Vec2(u1, u2));
        EXPECT_GE(L.z, 0.0);
        EXPECT_TRUE(std::isfinite(L.x));
        EXPECT_TRUE(std::isfinite(L.y));
        EXPECT_TRUE(std::isfinite(L.z));
    }
}

TEST(DisneyDiffuseSample, PDFIntegrationConsistency)
{
    DisneyDiffuse brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    const int N = 10000;
    double sum = 0.0;
    int valid = 0;
    for (int i = 0; i < N; ++i) {
        double u1 = (i + 0.5) / N;
        double u2 = std::fmod((i + 0.5) * 0.6180339887, 1.0);
        Vec3 L = brdf.sample(ctx, Vec2(u1, u2));
        if (L.z <= 0.0) continue;
        auto r = brdf.evaluate(ctx, L);
        if (r.pdf <= 0.0 || !std::isfinite(r.pdf)) continue;
        sum += r.value / r.pdf;
        ++valid;
    }
    EXPECT_GT(valid, N * 9 / 10); // most samples should be valid
    double mean = sum / valid;
    EXPECT_GT(mean, 0.0);
    EXPECT_TRUE(std::isfinite(mean));
}

// ---- contextAlpha and capabilities ----

TEST(DisneyDiffuseCapabilities, ContextAlpha)
{
    DisneyDiffuse brdf;
    // No squaring for Disney Diffuse
    EXPECT_DOUBLE_EQ(brdf.contextAlpha(0.5),  0.5);
    EXPECT_DOUBLE_EQ(brdf.contextAlpha(0.0),  0.0);
    EXPECT_DOUBLE_EQ(brdf.contextAlpha(1.0),  1.0);
}

TEST(DisneyDiffuseCapabilities, Name)
{
    DisneyDiffuse brdf;
    EXPECT_EQ(brdf.name(), "disney-diffuse");
}

TEST(DisneyDiffuseCapabilities, Capabilities)
{
    DisneyDiffuse brdf;
    auto caps = brdf.capabilities();
    EXPECT_FALSE(caps.supportsSchlickFresnel);
    EXPECT_EQ(caps.amplitudeModel, AmplitudeModel::Scalar);
    EXPECT_EQ(caps.parameterization, BRDFParameterization::PerceptualRoughness);
    EXPECT_EQ(caps.parameterName, "roughness");
}
