#include <gtest/gtest.h>
#include "ltc/NumericalIntegrator.h"
#include "ltc/GGX.h"
#include "ltc/Math.h"
#include <cmath>

using namespace ltc;

// A simple Lambertian BRDF: f_r = 1/pi, returns f_r * ndotl = ndotl/pi
struct LambertianBRDF : public IBRDF {
    std::string_view name() const override { return "lambertian"; }
    BRDFEvaluation evaluate(const BRDFContext&, const Vec3& L) const override {
        double ndotl = std::max(0.0, L.z);
        return { Scalar(ndotl) / Scalar(kPi), Scalar(ndotl) / Scalar(kPi) };
    }
    Vec3 sample(const BRDFContext&, const Vec2&) const override { return Vec3(0,0,1); }
    BRDFCapabilities capabilities() const override { return BRDFCapabilities{}; }
    double contextAlpha(double r) const override { return r; }
};

// A BRDF that always returns 0 (useful for testing below-horizon case)
struct ZeroBRDF : public IBRDF {
    std::string_view name() const override { return "zero"; }
    BRDFEvaluation evaluate(const BRDFContext&, const Vec3&) const override {
        return { Scalar(0), Scalar(0) };
    }
    Vec3 sample(const BRDFContext&, const Vec2&) const override { return Vec3(0,0,1); }
    BRDFCapabilities capabilities() const override { return BRDFCapabilities{}; }
    double contextAlpha(double r) const override { return r; }
};

// --- Test 1: Below-horizon light gives zero ---
TEST(NumericalIntegrator, BelowHorizonGivesZero)
{
    NumericalIntegrator integrator;
    LambertianBRDF brdf;

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // Polygon entirely at z = -2 (below the surface), facing toward shading point (+Z)
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-0.5,  0.5, -2),
        Vec3( 0.5,  0.5, -2),
        Vec3( 0.5, -0.5, -2),
        Vec3(-0.5, -0.5, -2),
    };

    BRDFContext ctx{Vec3(0,0,1), 0.5};
    NumericalIntegrationConfig cfg;
    cfg.sampleCount = 1024;

    NumericalIntegrationResult result = integrator.integrate(brdf, ctx, shading, light, cfg);

    EXPECT_NEAR(result.radiance.x, 0.0, 1e-10);
    EXPECT_NEAR(result.radiance.y, 0.0, 1e-10);
    EXPECT_NEAR(result.radiance.z, 0.0, 1e-10);
}

// --- Test 2: Positive result for frontal light ---
TEST(NumericalIntegrator, FrontalLightPositive)
{
    NumericalIntegrator integrator;
    GGX brdf;

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // Large frontal rectangle at z=1
    // Vertices in CCW order when viewed from BELOW (+z direction of shading point)
    // so the normal points toward -Z (toward the shading point)
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-2,  2, 1),  // CCW when viewed from below → normal = -Z (toward shading point)
        Vec3( 2,  2, 1),
        Vec3( 2, -2, 1),
        Vec3(-2, -2, 1),
    };

    BRDFContext ctx{Vec3(0,0,1), 0.5};
    NumericalIntegrationConfig cfg;
    cfg.sampleCount = 4096;

    NumericalIntegrationResult result = integrator.integrate(brdf, ctx, shading, light, cfg);

    EXPECT_GT(result.radiance.x, 0.0);
    EXPECT_GT(result.actualSamples, 0u);
    EXPECT_TRUE(result.converged);
}

// --- Test 3: Determinism ---
TEST(NumericalIntegrator, Deterministic)
{
    NumericalIntegrator integrator;
    GGX brdf;

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // CCW when viewed from below → normal = -Z (toward shading point)
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-0.5,  0.5, 2),
        Vec3( 0.5,  0.5, 2),
        Vec3( 0.5, -0.5, 2),
        Vec3(-0.5, -0.5, 2),
    };

    BRDFContext ctx{Vec3(0,0,1), 0.5};
    NumericalIntegrationConfig cfg;
    cfg.sampleCount = 1024;
    cfg.seed = 0;

    NumericalIntegrationResult r1 = integrator.integrate(brdf, ctx, shading, light, cfg);
    NumericalIntegrationResult r2 = integrator.integrate(brdf, ctx, shading, light, cfg);

    EXPECT_DOUBLE_EQ(r1.radiance.x, r2.radiance.x);
    EXPECT_DOUBLE_EQ(r1.radiance.y, r2.radiance.y);
    EXPECT_DOUBLE_EQ(r1.radiance.z, r2.radiance.z);
    EXPECT_EQ(r1.actualSamples, r2.actualSamples);
}

// --- Test 4: Finite and non-negative result ---
TEST(NumericalIntegrator, ResultIsFiniteNonNegative)
{
    NumericalIntegrator integrator;
    GGX brdf;

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // CCW when viewed from below → normal = -Z (facing shading point)
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-1,  1, 3),
        Vec3( 1,  1, 3),
        Vec3( 1, -1, 3),
        Vec3(-1, -1, 3),
    };

    BRDFContext ctx{Vec3(0,0,1), 0.3};
    NumericalIntegrationConfig cfg;
    cfg.sampleCount = 2048;

    NumericalIntegrationResult r = integrator.integrate(brdf, ctx, shading, light, cfg);

    EXPECT_TRUE(std::isfinite(r.radiance.x));
    EXPECT_TRUE(std::isfinite(r.radiance.y));
    EXPECT_TRUE(std::isfinite(r.radiance.z));
    EXPECT_GE(r.radiance.x, 0.0);
    EXPECT_GE(r.radiance.y, 0.0);
    EXPECT_GE(r.radiance.z, 0.0);
}

// --- Test 5: Degenerate polygon (less than 3 vertices) gives zero ---
TEST(NumericalIntegrator, DegeneratePolygon)
{
    NumericalIntegrator integrator;
    GGX brdf;

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = { Vec3(0,0,1), Vec3(1,0,1) };  // only 2 vertices

    BRDFContext ctx{Vec3(0,0,1), 0.5};
    NumericalIntegrationConfig cfg;
    cfg.sampleCount = 128;

    NumericalIntegrationResult r = integrator.integrate(brdf, ctx, shading, light, cfg);
    EXPECT_NEAR(r.radiance.x, 0.0, 1e-15);
    EXPECT_EQ(r.actualSamples, 0u);
}

// --- Test 6: Monochrome light produces equal RGB channels ---
TEST(NumericalIntegrator, MonochromeLightEqualChannels)
{
    NumericalIntegrator integrator;
    GGX brdf;

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // CCW when viewed from below → normal = -Z (facing shading point)
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-1,  1, 2),
        Vec3( 1,  1, 2),
        Vec3( 1, -1, 2),
        Vec3(-1, -1, 2),
    };

    BRDFContext ctx{Vec3(0,0,1), 0.5};
    NumericalIntegrationConfig cfg;
    cfg.sampleCount = 2048;

    NumericalIntegrationResult r = integrator.integrate(brdf, ctx, shading, light, cfg);

    // With white radiance (1,1,1) and an isotropic BRDF, all channels should be equal
    EXPECT_NEAR(r.radiance.x, r.radiance.y, 1e-14);
    EXPECT_NEAR(r.radiance.y, r.radiance.z, 1e-14);
}
