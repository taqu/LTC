#include <gtest/gtest.h>
#include "ltc/DistributionValidation.h"
#include "ltc/GGX.h"
#include "ltc/LTC.h"
#include "ltc/FittingResult.h"
#include <cmath>

using namespace ltc;

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
    cell.magnitude = 1.0 / kPi;  // Lambertian magnitude
    cell.fresnelCoefficient = 0.0;
    cell.hasFresnelCoefficient = false;
    cell.status = FitStatus::Converged;
    cell.completed = true;
    return cell;
}

// --- Test 1: L1 error is non-negative ---
TEST(DistributionValidation, L1ErrorNonNegative)
{
    GGX brdf;
    LUTCell cell = makeIdentityCell();

    LUTParameters params;
    params.alpha = 0.5;
    params.perceptualRoughness = std::sqrt(0.5);
    params.ndotv = 1.0;
    params.theta = 0.0;
    params.viewDirection = Vec3(0, 0, 1);

    DistributionValidationConfig cfg;
    cfg.sampleCount = 1024;  // small for speed

    DistributionErrorMetrics metrics = validateDistribution(brdf, cell, params, cfg);

    EXPECT_GE(metrics.l1Error, 0.0);
    EXPECT_GE(metrics.l2Error, 0.0);
    EXPECT_GE(metrics.cubicError, 0.0);
    EXPECT_GE(metrics.energyError, 0.0);
    EXPECT_GE(metrics.maxAbsError, 0.0);
}

// --- Test 2: L2 <= L1 (not necessarily true for all distributions, but for these small errors) ---
// Actually L2 can be larger than L1, so test that L2 is finite and positive
TEST(DistributionValidation, L2ErrorFinite)
{
    GGX brdf;
    LUTCell cell = makeIdentityCell();

    LUTParameters params;
    params.alpha = 1.0;
    params.perceptualRoughness = 1.0;
    params.ndotv = 1.0;
    params.theta = 0.0;
    params.viewDirection = Vec3(0, 0, 1);

    DistributionValidationConfig cfg;
    cfg.sampleCount = 1024;

    DistributionErrorMetrics metrics = validateDistribution(brdf, cell, params, cfg);

    EXPECT_TRUE(std::isfinite(metrics.l1Error));
    EXPECT_TRUE(std::isfinite(metrics.l2Error));
    EXPECT_GT(metrics.sampleCount, 0u);
}

// --- Test 3: Max error >= 0 ---
TEST(DistributionValidation, MaxErrorNonNegative)
{
    GGX brdf;
    LUTCell cell = makeIdentityCell();

    LUTParameters params;
    params.alpha = 0.3;
    params.perceptualRoughness = std::sqrt(0.3);
    params.ndotv = 0.8;
    double sinv = std::sqrt(1.0 - 0.8 * 0.8);
    params.theta = std::acos(0.8);
    params.viewDirection = Vec3(sinv, 0, 0.8);

    DistributionValidationConfig cfg;
    cfg.sampleCount = 1024;

    DistributionErrorMetrics metrics = validateDistribution(brdf, cell, params, cfg);

    EXPECT_GE(metrics.maxAbsError, 0.0);
}

// --- Test 4: Sample count is populated correctly ---
TEST(DistributionValidation, SampleCountCorrect)
{
    GGX brdf;
    LUTCell cell = makeIdentityCell();

    LUTParameters params;
    params.alpha = 0.5;
    params.perceptualRoughness = std::sqrt(0.5);
    params.ndotv = 1.0;
    params.theta = 0.0;
    params.viewDirection = Vec3(0, 0, 1);

    DistributionValidationConfig cfg;
    cfg.sampleCount = 2500;  // 50*50

    DistributionErrorMetrics metrics = validateDistribution(brdf, cell, params, cfg);

    // sqrtN = 50, count = 50*50 = 2500
    EXPECT_EQ(metrics.sampleCount, 2500u);
}

// --- Test 5: Max error direction is valid (finite) ---
TEST(DistributionValidation, MaxErrorDirectionFinite)
{
    GGX brdf;
    LUTCell cell = makeIdentityCell();

    LUTParameters params;
    params.alpha = 0.5;
    params.perceptualRoughness = std::sqrt(0.5);
    params.ndotv = 1.0;
    params.theta = 0.0;
    params.viewDirection = Vec3(0, 0, 1);

    DistributionValidationConfig cfg;
    cfg.sampleCount = 1024;

    DistributionErrorMetrics metrics = validateDistribution(brdf, cell, params, cfg);

    EXPECT_TRUE(std::isfinite(metrics.maxErrorDirection.x));
    EXPECT_TRUE(std::isfinite(metrics.maxErrorDirection.y));
    EXPECT_TRUE(std::isfinite(metrics.maxErrorDirection.z));
}

// --- Test 6: Identity LTC has bounded error vs GGX at alpha=1.0 ---
TEST(DistributionValidation, IdentityLTCBoundedErrorAtHighAlpha)
{
    GGX brdf;
    LUTCell cell = makeIdentityCell();

    LUTParameters params;
    params.alpha = 1.0;
    params.perceptualRoughness = 1.0;
    params.ndotv = 1.0;
    params.theta = 0.0;
    params.viewDirection = Vec3(0, 0, 1);

    DistributionValidationConfig cfg;
    cfg.sampleCount = 4096;

    DistributionErrorMetrics metrics = validateDistribution(brdf, cell, params, cfg);

    // Error should be finite and bounded — identity LTC at alpha=1 is a rough fit
    EXPECT_TRUE(std::isfinite(metrics.l1Error));
    EXPECT_LT(metrics.l1Error, 2.0);  // generous bound
}
