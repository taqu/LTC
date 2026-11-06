#include <gtest/gtest.h>
#include "ltc/Validation.h"
#include "ltc/LTCPolygonIntegrator.h"
#include "ltc/LUT.h"
#include "ltc/LTC.h"
#include "ltc/FittingResult.h"
#include "ltc/ParameterGrid.h"
#include "ltc/ValidationConfig.h"
#include <cmath>

using namespace ltc;

namespace {

// Build a LUTData filled with identity-like cells
static LUTData makeIdentityLUT(uint32_t W, uint32_t H)
{
    LUTMetadata meta;
    meta.brdfName  = "GGX";
    meta.width     = W;
    meta.height    = H;
    LUTData lut(W, H, meta);

    ParameterGrid grid(W, H);
    for (uint32_t vi = 0; vi < H; ++vi) {
        for (uint32_t ri = 0; ri < W; ++ri) {
            LUTCell& cell = lut.at(ri, vi);
            cell.parameters.m11 = 1.0;
            cell.parameters.m22 = 1.0;
            cell.parameters.m13 = 0.0;
            cell.parameters.basis = Mat3(1.0);

            LTC ltc;
            ltc.setParameters(1.0, 1.0, 0.0);
            cell.matrix = ltc.matrix();
            cell.inverseMatrix = ltc.inverseMatrix();
            cell.magnitude = 1.0;
            cell.fresnelCoefficient = 0.0;
            cell.hasFresnelCoefficient = false;
            cell.status = FitStatus::Converged;
            cell.completed = true;
        }
    }
    return lut;
}

static LUTCell makeIdentityCell()
{
    LUTCell cell;
    cell.parameters.m11 = 1.0;
    cell.parameters.m22 = 1.0;
    cell.parameters.m13 = 0.0;
    cell.parameters.basis = Mat3(1.0);
    LTC ltc;
    ltc.setParameters(1.0, 1.0, 0.0);
    cell.matrix = ltc.matrix();
    cell.inverseMatrix = ltc.inverseMatrix();
    cell.magnitude = 1.0;
    cell.fresnelCoefficient = 0.0;
    cell.hasFresnelCoefficient = false;
    cell.status = FitStatus::Converged;
    cell.completed = true;
    return cell;
}

} // anonymous namespace

// --- Test 1: Below-horizon light gives zero via LTC polygon integrator ---
TEST(LightingValidation, BelowHorizonGivesZero)
{
    LTCPolygonIntegrator integrator;
    LUTCell cell = makeIdentityCell();

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-0.5, -0.5, -2),
        Vec3( 0.5, -0.5, -2),
        Vec3( 0.5,  0.5, -2),
        Vec3(-0.5,  0.5, -2),
    };

    LTCIntegrationResult r = integrator.evaluate(cell, shading, light);

    EXPECT_NEAR(r.radiance.x, 0.0, 1e-10);
    EXPECT_NEAR(r.radiance.y, 0.0, 1e-10);
    EXPECT_NEAR(r.radiance.z, 0.0, 1e-10);
}

// --- Test 2: Above-horizon frontal light gives positive result ---
TEST(LightingValidation, AboveHorizonPositive)
{
    LTCPolygonIntegrator integrator;
    LUTCell cell = makeIdentityCell();

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-2, -2, 2),
        Vec3( 2, -2, 2),
        Vec3( 2,  2, 2),
        Vec3(-2,  2, 2),
    };

    LTCIntegrationResult r = integrator.evaluate(cell, shading, light);

    EXPECT_GT(r.radiance.x, 0.0);
    EXPECT_FALSE(r.degenerate);
}

// --- Test 3: Lighting validation runner produces results ---
TEST(LightingValidation, RunnerProducesResults)
{
    LUTData lut = makeIdentityLUT(8, 8);
    ValidationConfig cfg;
    cfg.lightingSamples = 512;  // small for speed
    cfg.thresholds.lightingRelative = 0.5;  // generous threshold
    cfg.thresholds.lightingAbsolute = 0.001;

    ValidationRunner runner;
    auto results = runner.runLightingValidation(lut, cfg);

    // Should have all 8 built-in scenarios
    EXPECT_GE(results.size(), 8u);

    // All results should have valid IDs
    for (const auto& r : results) {
        EXPECT_FALSE(r.id.empty());
        EXPECT_TRUE(std::isfinite(r.ltcResult));
        EXPECT_TRUE(std::isfinite(r.absError));
    }
}

// --- Test 4: Below-horizon scenario has near-zero LTC result ---
TEST(LightingValidation, BelowHorizonScenarioNearZero)
{
    LUTData lut = makeIdentityLUT(8, 8);
    ValidationConfig cfg;
    cfg.lightingSamples = 512;

    ValidationRunner runner;
    auto results = runner.runLightingValidation(lut, cfg);

    // Find the below-horizon scenario
    bool found = false;
    for (const auto& r : results) {
        if (r.id == "below-horizon") {
            EXPECT_NEAR(r.ltcResult, 0.0, 1e-6);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "below-horizon scenario not found";
}

// --- Test 5: Frontal rectangle scenario has positive LTC result ---
TEST(LightingValidation, FrontalRectanglePositive)
{
    LUTData lut = makeIdentityLUT(8, 8);
    ValidationConfig cfg;
    cfg.lightingSamples = 512;

    ValidationRunner runner;
    auto results = runner.runLightingValidation(lut, cfg);

    bool found = false;
    for (const auto& r : results) {
        if (r.id == "large-frontal-rectangle") {
            EXPECT_GT(r.ltcResult, 0.0);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "large-frontal-rectangle scenario not found";
}

// --- Test 6: Empty LUT produces empty results ---
TEST(LightingValidation, EmptyLUTProducesEmptyResults)
{
    LUTData emptyLut;
    ValidationConfig cfg;

    ValidationRunner runner;
    auto results = runner.runLightingValidation(emptyLut, cfg);

    EXPECT_TRUE(results.empty());
}

// --- Test 7: TwoSided light vs one-sided ---
TEST(LightingValidation, TwoSidedVsOneSided)
{
    LTCPolygonIntegrator integrator;
    LUTCell cell = makeIdentityCell();

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // Rectangle facing UP (normal = +Z), positioned above shading point
    // When viewed from below (shading point at z=0, rect at z=2),
    // the back-face of the polygon faces toward the shading point
    PolygonAreaLight lightOneSided;
    lightOneSided.radiance = Vec3(1,1,1);
    lightOneSided.twoSided = false;
    // CCW winding when viewed from above (+Z direction)
    // This means the front face is pointing up (away from shading point)
    lightOneSided.vertices = {
        Vec3(-1,  1, 2),
        Vec3( 1,  1, 2),
        Vec3( 1, -1, 2),
        Vec3(-1, -1, 2),
    };

    PolygonAreaLight lightTwoSided = lightOneSided;
    lightTwoSided.twoSided = true;

    // The LTC integrator doesn't use the polygon normal directly for
    // emission direction (that's handled by the shading frame and clipping).
    // Both should give the same result from the LTC perspective since the
    // LTC only clips against the horizon.
    LTCIntegrationResult r1 = integrator.evaluate(cell, shading, lightOneSided);
    LTCIntegrationResult r2 = integrator.evaluate(cell, shading, lightTwoSided);

    // Both are geometric operations (horizon clip) in LTC space —
    // the LTC integrator doesn't distinguish one/two-sided emission direction
    // (that's a property of the numerical integrator).
    // So both should give the same result since the polygon is above horizon.
    EXPECT_NEAR(r1.radiance.x, r2.radiance.x, 1e-10);
}
