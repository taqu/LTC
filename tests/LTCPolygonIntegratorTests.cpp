#include <gtest/gtest.h>
#include "ltc/LTCPolygonIntegrator.h"
#include "ltc/LTC.h"
#include "ltc/Math.h"
#include "ltc/FittingResult.h"
#include <cmath>

using namespace ltc;

// Helper: build an identity-LTC LUTCell
static LUTCell makeIdentityCell()
{
    LUTCell cell;
    cell.parameters.m11 = 1.0;
    cell.parameters.m22 = 1.0;
    cell.parameters.m13 = 0.0;
    cell.parameters.basis = Mat3(1.0); // identity basis

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

// --- Test 1: Below-horizon polygon gives zero ---
TEST(LTCPolygonIntegrator, BelowHorizonGivesZero)
{
    LTCPolygonIntegrator integrator;
    LUTCell cell = makeIdentityCell();

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // Rectangle entirely below horizon
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-1, -1, -2),
        Vec3( 1, -1, -2),
        Vec3( 1,  1, -2),
        Vec3(-1,  1, -2),
    };

    LTCIntegrationResult result = integrator.evaluate(cell, shading, light);

    EXPECT_NEAR(result.radiance.x, 0.0, 1e-10);
    EXPECT_NEAR(result.radiance.y, 0.0, 1e-10);
    EXPECT_NEAR(result.radiance.z, 0.0, 1e-10);
}

// --- Test 2: Frontal polygon above horizon gives positive result ---
TEST(LTCPolygonIntegrator, FrontalPolygonPositive)
{
    LTCPolygonIntegrator integrator;
    LUTCell cell = makeIdentityCell();

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // Large rectangle facing down at z=2
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-2, -2, 2),
        Vec3( 2, -2, 2),
        Vec3( 2,  2, 2),
        Vec3(-2,  2, 2),
    };

    LTCIntegrationResult result = integrator.evaluate(cell, shading, light);

    EXPECT_GT(result.radiance.x, 0.0);
    EXPECT_FALSE(result.degenerate);
}

// --- Test 3: Partial horizon clipping produces valid result ---
TEST(LTCPolygonIntegrator, PartialHorizonClip)
{
    LTCPolygonIntegrator integrator;
    LUTCell cell = makeIdentityCell();

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // Rectangle straddling z=0 (half above, half below)
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-1, -1, -1),  // below
        Vec3( 1, -1, -1),  // below
        Vec3( 1,  1,  1),  // above
        Vec3(-1,  1,  1),  // above
    };

    LTCIntegrationResult result = integrator.evaluate(cell, shading, light);

    // Should be positive (some above horizon) and finite
    EXPECT_GE(result.radiance.x, 0.0);
    EXPECT_TRUE(std::isfinite(result.radiance.x));
}

// --- Test 4: Small polygon far away gives small positive result ---
TEST(LTCPolygonIntegrator, SmallFarPolygonSmallResult)
{
    LTCPolygonIntegrator integrator;
    LUTCell cell = makeIdentityCell();

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // Tiny rectangle far away
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-0.01, -0.01, 100),
        Vec3( 0.01, -0.01, 100),
        Vec3( 0.01,  0.01, 100),
        Vec3(-0.01,  0.01, 100),
    };

    LTCIntegrationResult result = integrator.evaluate(cell, shading, light);

    EXPECT_GT(result.radiance.x, 0.0);
    EXPECT_LT(result.radiance.x, 0.01);  // should be small
}

// --- Test 5: Magnitude scaling works ---
TEST(LTCPolygonIntegrator, MagnitudeScaling)
{
    LTCPolygonIntegrator integrator;

    // Build two cells identical except for magnitude
    LUTCell cell1 = makeIdentityCell();
    cell1.magnitude = 1.0;

    LUTCell cell2 = makeIdentityCell();
    cell2.magnitude = 2.0;

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-1, -1, 2),
        Vec3( 1, -1, 2),
        Vec3( 1,  1, 2),
        Vec3(-1,  1, 2),
    };

    LTCIntegrationResult r1 = integrator.evaluate(cell1, shading, light);
    LTCIntegrationResult r2 = integrator.evaluate(cell2, shading, light);

    // magnitude=2 should give twice the radiance
    EXPECT_NEAR(r2.radiance.x, 2.0 * r1.radiance.x, 1e-10);
}

// --- Test 6: evaluateWithMatrix matches evaluate ---
TEST(LTCPolygonIntegrator, EvaluateWithMatrixMatchesEvaluate)
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
        Vec3(-1, -1, 2),
        Vec3( 1, -1, 2),
        Vec3( 1,  1, 2),
        Vec3(-1,  1, 2),
    };

    LTCIntegrationResult r1 = integrator.evaluate(cell, shading, light);
    LTCIntegrationResult r2 = integrator.evaluateWithMatrix(
        cell.inverseMatrix, cell.magnitude, shading, light);

    EXPECT_NEAR(r1.radiance.x, r2.radiance.x, 1e-12);
    EXPECT_NEAR(r1.radiance.y, r2.radiance.y, 1e-12);
    EXPECT_NEAR(r1.radiance.z, r2.radiance.z, 1e-12);
}

// --- Test 7: Monochromatic light gives equal channels ---
TEST(LTCPolygonIntegrator, MonochromaticLightEqualChannels)
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
        Vec3(-1, -1, 2),
        Vec3( 1, -1, 2),
        Vec3( 1,  1, 2),
        Vec3(-1,  1, 2),
    };

    LTCIntegrationResult r = integrator.evaluate(cell, shading, light);

    EXPECT_NEAR(r.radiance.x, r.radiance.y, 1e-14);
    EXPECT_NEAR(r.radiance.y, r.radiance.z, 1e-14);
}
