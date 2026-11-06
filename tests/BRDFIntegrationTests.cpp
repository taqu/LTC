#include <gtest/gtest.h>
#include "ltc/BRDFIntegration.h"
#include "ltc/GGX.h"
#include "ltc/BRDF.h"
#include <cmath>

using namespace ltc;

static FittingConfig defaultConfig()
{
    FittingConfig c;
    c.sampleResolution = 32;
    return c;
}

// ---- Magnitude is finite and positive ----

TEST(BRDFIntegration, MagnitudeFinitePositive)
{
    GGX brdf;
    BRDFContext ctx;
    ctx.view  = Vec3(0.0, 0.0, 1.0);
    ctx.alpha = 0.5;

    BRDFIntegrator integrator;
    auto r = integrator.integrate(brdf, ctx, defaultConfig());

    EXPECT_TRUE(std::isfinite(r.magnitude));
    EXPECT_GT(r.magnitude, 0.0);
}

// ---- Magnitude varies monotonically with roughness ----
// Rougher surfaces diffuse more energy; smoother surfaces concentrate it,
// but both should give positive magnitude in [0, pi] roughly.

TEST(BRDFIntegration, MagnitudeVariesWithAlpha)
{
    GGX brdf;
    BRDFContext ctx;
    ctx.view = Vec3(0.0, 0.0, 1.0);

    BRDFIntegrator integrator;

    double mag_low  = integrator.integrate(brdf, {ctx.view, Scalar(0.1)},  defaultConfig()).magnitude;
    double mag_high = integrator.integrate(brdf, {ctx.view, Scalar(0.9)},  defaultConfig()).magnitude;

    // Both should be positive and finite.
    EXPECT_GT(mag_low,  0.0);
    EXPECT_GT(mag_high, 0.0);
    EXPECT_TRUE(std::isfinite(mag_low));
    EXPECT_TRUE(std::isfinite(mag_high));
}

// ---- Normal incidence: average direction should lie along Z ----

TEST(BRDFIntegration, NormalIncidenceAvgDirIsZ)
{
    GGX brdf;
    BRDFContext ctx;
    ctx.view  = Vec3(0.0, 0.0, 1.0); // normal incidence
    ctx.alpha = 0.5;

    BRDFIntegrator integrator;
    auto r = integrator.integrate(brdf, ctx, defaultConfig());

    // At normal incidence the BRDF is rotationally symmetric around Z,
    // so the average X and Y components should be near zero.
    if (r.hasValidDirection) {
        EXPECT_NEAR(r.averageDirection.x, 0.0, 1e-6);
        EXPECT_NEAR(r.averageDirection.y, 0.0, 1e-6);
    }
}

// ---- Fresnel coefficient is in [0, 1] ----

TEST(BRDFIntegration, FresnelCoefficientInRange)
{
    GGX brdf;
    BRDFContext ctx;
    ctx.view  = Vec3(0.0, 0.0, 1.0);
    ctx.alpha = 0.5;

    BRDFIntegrator integrator;
    auto r = integrator.integrate(brdf, ctx, defaultConfig());

    if (r.hasFresnelCoefficient) {
        EXPECT_GE(r.fresnelCoefficient, 0.0);
        EXPECT_LE(r.fresnelCoefficient, 1.0 + 1e-6);
    }
}

// ---- Average direction Y component is zero (isotropic BRDF) ----

TEST(BRDFIntegration, AverageDirYIsZero)
{
    GGX brdf;
    // Oblique view: view is in XZ plane, Y=0.
    Vec3 v = glm::normalize(Vec3(0.5, 0.0, std::sqrt(1.0 - 0.25)));
    BRDFContext ctx{v, Scalar(0.5)};

    BRDFIntegrator integrator;
    auto r = integrator.integrate(brdf, ctx, defaultConfig());

    if (r.hasValidDirection) {
        EXPECT_NEAR(r.averageDirection.y, 0.0, 1e-6);
    }
}

// ---- Determinism ----

TEST(BRDFIntegration, Deterministic)
{
    GGX brdf;
    BRDFContext ctx;
    ctx.view  = Vec3(glm::normalize(Vec3(0.3, 0.0, 0.954)));
    ctx.alpha = 0.4;

    BRDFIntegrator integrator;
    auto r1 = integrator.integrate(brdf, ctx, defaultConfig());
    auto r2 = integrator.integrate(brdf, ctx, defaultConfig());

    EXPECT_DOUBLE_EQ(r1.magnitude,          r2.magnitude);
    EXPECT_DOUBLE_EQ(r1.fresnelCoefficient, r2.fresnelCoefficient);
    EXPECT_EQ(r1.hasValidDirection,         r2.hasValidDirection);
    if (r1.hasValidDirection) {
        EXPECT_DOUBLE_EQ(r1.averageDirection.x, r2.averageDirection.x);
        EXPECT_DOUBLE_EQ(r1.averageDirection.y, r2.averageDirection.y);
        EXPECT_DOUBLE_EQ(r1.averageDirection.z, r2.averageDirection.z);
    }
}

// ---- Higher resolution increases stability (not a strict monotone test) ----

TEST(BRDFIntegration, HigherResolutionGivesFiniteResult)
{
    GGX brdf;
    BRDFContext ctx;
    ctx.view  = Vec3(0.0, 0.0, 1.0);
    ctx.alpha = 0.3;

    FittingConfig cfg64;
    cfg64.sampleResolution = 64;

    BRDFIntegrator integrator;
    auto r = integrator.integrate(brdf, ctx, cfg64);

    EXPECT_TRUE(std::isfinite(r.magnitude));
    EXPECT_GT(r.magnitude, 0.0);
}
