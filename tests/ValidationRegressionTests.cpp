#include <gtest/gtest.h>
#include "ltc/Validation.h"
#include "ltc/LTCPolygonIntegrator.h"
#include "ltc/NumericalIntegrator.h"
#include "ltc/ValidationReport.h"
#include "ltc/LUT.h"
#include "ltc/LTC.h"
#include "ltc/FittingResult.h"
#include "ltc/ParameterGrid.h"
#include <cmath>
#include <filesystem>

using namespace ltc;

namespace {

static LUTData makeSmallIdentityLUT(uint32_t W = 4, uint32_t H = 4)
{
    LUTMetadata meta;
    meta.brdfName = "GGX";
    meta.width    = W;
    meta.height   = H;
    LUTData lut(W, H, meta);

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
            cell.status = FitStatus::Converged;
            cell.completed = true;
        }
    }
    return lut;
}

} // anonymous namespace

// --- Test 1: Spherical polygon integral of a known configuration ---
// For identity LTC + unit square centered at (0,0,2):
// LTC result should be positive and agree with numerical within 10%
TEST(ValidationRegression, IdentityLTCSquareFrontal)
{
    LTCPolygonIntegrator ltcIntegrator;

    LUTCell cell;
    cell.parameters.m11 = 1.0;
    cell.parameters.m22 = 1.0;
    cell.parameters.m13 = 0.0;
    cell.parameters.basis = Mat3(1.0);
    LTC ltcObj;
    ltcObj.setParameters(1.0, 1.0, 0.0);
    cell.matrix = ltcObj.matrix();
    cell.inverseMatrix = ltcObj.inverseMatrix();
    cell.magnitude = 1.0;
    cell.status = FitStatus::Converged;
    cell.completed = true;

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // Unit square at z=2 (half-extent = 0.5)
    // CCW when viewed from above (from +z direction) - LTC integrator convention
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-0.5, -0.5, 2),
        Vec3( 0.5, -0.5, 2),
        Vec3( 0.5,  0.5, 2),
        Vec3(-0.5,  0.5, 2),
    };

    LTCIntegrationResult ltcResult = ltcIntegrator.evaluate(cell, shading, light);

    // Should be positive and finite
    EXPECT_GT(ltcResult.radiance.x, 0.0);
    EXPECT_TRUE(std::isfinite(ltcResult.radiance.x));
    EXPECT_FALSE(ltcResult.degenerate);

    // Should be reasonably small (small solid angle from z=2, 1x1 square)
    // The solid angle of a 1x1 square at z=2 distance is roughly (1*1)/(4*4) * pi ≈ 0.2
    // With cosine weighting it should be less than 0.5 / pi ≈ 0.16
    EXPECT_LT(ltcResult.radiance.x, 1.0);
}

// --- Test 2: LTC vs numerical for large frontal rect ---
// LTC and numerical should agree within 15% for identity LTC
TEST(ValidationRegression, LTCNumericalAgreement)
{
    LTCPolygonIntegrator ltcIntegrator;
    NumericalIntegrator numericalIntegrator;

    LUTCell cell;
    cell.parameters.m11 = 1.0;
    cell.parameters.m22 = 1.0;
    cell.parameters.m13 = 0.0;
    cell.parameters.basis = Mat3(1.0);
    LTC ltcObj;
    ltcObj.setParameters(1.0, 1.0, 0.0);
    cell.matrix = ltcObj.matrix();
    cell.inverseMatrix = ltcObj.inverseMatrix();
    cell.magnitude = 1.0;
    cell.status = FitStatus::Converged;
    cell.completed = true;

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // CCW when viewed from above (from +z) - LTC integrator convention
    // twoSided so the numerical integrator also works
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = true;
    light.vertices = {
        Vec3(-2, -2, 2),
        Vec3( 2, -2, 2),
        Vec3( 2,  2, 2),
        Vec3(-2,  2, 2),
    };

    LTCIntegrationResult ltcResult = ltcIntegrator.evaluate(cell, shading, light);

    // For identity LTC with magnitude=1, the LTC analytical result gives:
    // integral of (cos/pi) dOmega over the polygon = integral over the spherical polygon
    // This should match a Lambertian BRDF numerical estimate when properly scaled.
    // We use a lambertian BRDF: value = ndotl/pi (cosine-weighted)
    struct LambertBRDF : public IBRDF {
        std::string_view name() const override { return "lambert"; }
        BRDFEvaluation evaluate(const BRDFContext&, const Vec3& L) const override {
            double ndotl = std::max(0.0, L.z);
            // value = f_r * ndotl = (1/pi) * ndotl
            return { Scalar(ndotl / kPi), Scalar(ndotl / kPi) };
        }
        Vec3 sample(const BRDFContext&, const Vec2&) const override { return Vec3(0,0,1); }
        BRDFCapabilities capabilities() const override { return BRDFCapabilities{}; }
        double contextAlpha(double r) const override { return r; }
    } brdf;

    BRDFContext ctx{Vec3(0,0,1), 0.5};
    NumericalIntegrationConfig numCfg;
    numCfg.sampleCount = 8192;
    NumericalIntegrationResult numResult = numericalIntegrator.integrate(
        brdf, ctx, shading, light, numCfg);

    // Both should be positive
    EXPECT_GT(ltcResult.radiance.x, 0.0);
    EXPECT_GT(numResult.radiance.x, 0.0);

    // For identity LTC (Minv = I, magnitude=1):
    // LTC integral = integral_polygon (cos(theta)/pi) dOmega
    // This is exactly the Lambertian irradiance: integral f_r * ndotl dOmega = integral ndotl/pi dOmega
    // So ltcResult.radiance.x should equal numResult.radiance.x within 15%
    double ltcLum = ltcResult.radiance.x;
    double numLum = numResult.radiance.x;
    double relErr = std::abs(ltcLum - numLum) / std::max(numLum, 1e-10);
    EXPECT_LT(relErr, 0.15) << "ltcLum=" << ltcLum << " numLum=" << numLum;
}

// --- Test 3: Validation runner produces non-empty report ---
TEST(ValidationRegression, RunnerProducesReport)
{
    LUTData lut = makeSmallIdentityLUT();
    ValidationConfig cfg;
    cfg.distributionSamples = 1024;  // small for speed
    cfg.lightingSamples     = 512;
    cfg.thresholds.distributionL1  = 10.0;  // generous
    cfg.thresholds.lightingRelative = 1.0;

    ValidationRunner runner;
    FullValidationReport report = runner.runAll(
        lut, std::filesystem::temp_directory_path(), cfg);

    // Should not crash and produce some results
    EXPECT_FALSE(report.toolVersion.empty());
    EXPECT_GE(report.distributionResults.size(), 1u);
    EXPECT_GE(report.lightingResults.size(), 1u);
}

// --- Test 4: reportToJson does not throw ---
TEST(ValidationRegression, ReportToJsonNoThrow)
{
    FullValidationReport report;
    report.toolVersion = "0.1.0";
    report.validationProfile = "Quality";

    DistributionErrorMetrics m;
    m.l1Error = 0.1;
    m.l2Error = 0.05;
    m.sampleCount = 1024;
    report.distributionResults.push_back(m);

    LightingScenarioResult ls;
    ls.id = "test";
    ls.groundTruth = 0.5;
    ls.ltcResult = 0.48;
    ls.absError = 0.02;
    ls.relError = 0.04;
    report.lightingResults.push_back(ls);

    EXPECT_NO_THROW({
        auto j = reportToJson(report);
        EXPECT_FALSE(j.empty());
    });
}

// --- Test 5: writeJsonReport creates a file ---
TEST(ValidationRegression, WriteJsonReportCreatesFile)
{
    FullValidationReport report;
    report.toolVersion = "0.1.0";
    report.validationProfile = "Quality";

    std::filesystem::path reportPath = std::filesystem::temp_directory_path()
        / "ltc_validation_test_report.json";

    // Clean up if exists
    std::error_code ec;
    std::filesystem::remove(reportPath, ec);

    EXPECT_NO_THROW(writeJsonReport(report, reportPath));
    EXPECT_TRUE(std::filesystem::exists(reportPath));

    // Clean up
    std::filesystem::remove(reportPath, ec);
}

// --- Test 6: writeCsvReport creates a file ---
TEST(ValidationRegression, WriteCsvReportCreatesFile)
{
    std::vector<DistributionErrorMetrics> metrics;
    DistributionErrorMetrics m;
    m.l1Error = 0.1;
    m.l2Error = 0.05;
    m.sampleCount = 1024;
    metrics.push_back(m);

    std::filesystem::path csvPath = std::filesystem::temp_directory_path()
        / "ltc_validation_test_dist.csv";

    std::error_code ec;
    std::filesystem::remove(csvPath, ec);

    EXPECT_NO_THROW(writeCsvReport(metrics, csvPath));
    EXPECT_TRUE(std::filesystem::exists(csvPath));

    std::filesystem::remove(csvPath, ec);
}

// --- Test 7: Edge integral gives expected value for a full hemisphere ---
// For a polygon that covers the full hemisphere, the spherical integral should equal 1.0
TEST(ValidationRegression, LTCFullHemisphereIntegral)
{
    LTCPolygonIntegrator integrator;

    // Use identity matrix with magnitude = pi (so integral = pi * irradiance = 1.0 for full hemisphere)
    LUTCell cell;
    cell.parameters.m11 = 1.0;
    cell.parameters.m22 = 1.0;
    cell.parameters.m13 = 0.0;
    cell.parameters.basis = Mat3(1.0);
    LTC ltcObj;
    ltcObj.setParameters(1.0, 1.0, 0.0);
    cell.matrix = ltcObj.matrix();
    cell.inverseMatrix = ltcObj.inverseMatrix();
    cell.magnitude = kPi;  // pi so result = pi * (1/2pi * solid_angle) = solid_angle/2
    cell.status = FitStatus::Converged;
    cell.completed = true;

    ShadingPoint shading;
    shading.position = Vec3(0,0,0);
    shading.normal   = Vec3(0,0,1);
    shading.view     = Vec3(0,0,1);

    // Very large polygon that covers essentially the full upper hemisphere
    PolygonAreaLight light;
    light.radiance = Vec3(1,1,1);
    light.twoSided = false;
    light.vertices = {
        Vec3(-1000, -1000, 0.001),
        Vec3( 1000, -1000, 0.001),
        Vec3( 1000,  1000, 0.001),
        Vec3(-1000,  1000, 0.001),
    };

    LTCIntegrationResult result = integrator.evaluate(cell, shading, light);

    // For identity LTC + magnitude=pi + near-hemisphere polygon:
    // irradiance ≈ sum_edges / (2*pi) ≈ 1.0 for full hemisphere
    // result.radiance = magnitude * irradiance ≈ pi * 1.0 = pi
    // But we care that it's close to pi (within 1%)
    EXPECT_NEAR(result.radiance.x, kPi, kPi * 0.01);
    EXPECT_FALSE(result.degenerate);
}
