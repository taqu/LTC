#include "ltc/Validation.h"
#include "ltc/LTCPolygonIntegrator.h"
#include "ltc/NumericalIntegrator.h"
#include "ltc/GGX.h"
#include "ltc/LTC.h"
#include "ltc/ParameterGrid.h"
#include <algorithm>
#include <cmath>

namespace ltc {

namespace {

// Bilinear interpolation of LUT cells given alpha and ndotv
LUTCell interpolateLUTCell(const LUTData& lut, double alpha, double ndotv)
{
    uint32_t W = lut.width();
    uint32_t H = lut.height();

    if (W == 0 || H == 0) return LUTCell{};

    // Map (alpha, ndotv) → fractional LUT coordinates
    // Roughness: r = sqrt(alpha), u = r * (W-1)
    // View: x = sqrt(1 - ndotv), v = x * (H-1)
    double r = std::sqrt(std::max(0.0, alpha));
    double x = std::sqrt(std::max(0.0, 1.0 - std::max(0.0, std::min(1.0, ndotv))));

    double u = r * (W - 1);
    double v = x * (H - 1);

    int ri0 = std::max(0, std::min((int)W - 1, (int)std::floor(u)));
    int ri1 = std::max(0, std::min((int)W - 1, ri0 + 1));
    int vi0 = std::max(0, std::min((int)H - 1, (int)std::floor(v)));
    int vi1 = std::max(0, std::min((int)H - 1, vi0 + 1));

    double fu = u - std::floor(u);
    double fv = v - std::floor(v);
    // Clamp fractions to [0,1]
    fu = std::max(0.0, std::min(1.0, fu));
    fv = std::max(0.0, std::min(1.0, fv));

    const LUTCell& c00 = lut.at((uint32_t)ri0, (uint32_t)vi0);
    const LUTCell& c10 = lut.at((uint32_t)ri1, (uint32_t)vi0);
    const LUTCell& c01 = lut.at((uint32_t)ri0, (uint32_t)vi1);
    const LUTCell& c11 = lut.at((uint32_t)ri1, (uint32_t)vi1);

    auto lerp = [](double a, double b, double t) { return a + t * (b - a); };

    // Bilinear interpolation of scalar parameters
    auto bilerp = [&](double v00, double v10, double v01, double v11) {
        return lerp(lerp(v00, v10, fu), lerp(v01, v11, fu), fv);
    };

    LUTCell cell;
    cell.parameters.m11 = bilerp(c00.parameters.m11, c10.parameters.m11,
                                  c01.parameters.m11, c11.parameters.m11);
    cell.parameters.m22 = bilerp(c00.parameters.m22, c10.parameters.m22,
                                  c01.parameters.m22, c11.parameters.m22);
    cell.parameters.m13 = bilerp(c00.parameters.m13, c10.parameters.m13,
                                  c01.parameters.m13, c11.parameters.m13);

    // Interpolate basis columns
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            cell.parameters.basis[col][row] = bilerp(
                c00.parameters.basis[col][row],
                c10.parameters.basis[col][row],
                c01.parameters.basis[col][row],
                c11.parameters.basis[col][row]);
        }
    }

    cell.magnitude = bilerp(c00.magnitude, c10.magnitude, c01.magnitude, c11.magnitude);
    cell.fresnelCoefficient = bilerp(c00.fresnelCoefficient, c10.fresnelCoefficient,
                                      c01.fresnelCoefficient, c11.fresnelCoefficient);
    cell.hasFresnelCoefficient = c00.hasFresnelCoefficient;
    cell.status = c00.status;
    cell.completed = true;

    // Recompute matrices from interpolated parameters
    LTC ltc;
    ltc.setParameters(
        Scalar(cell.parameters.m11),
        Scalar(cell.parameters.m22),
        Scalar(cell.parameters.m13));

    // Normalize basis columns before setting
    Vec3 bX = cell.parameters.basis[0];
    Vec3 bY = cell.parameters.basis[1];
    Vec3 bZ = cell.parameters.basis[2];
    double lenX = glm::length(bX);
    double lenY = glm::length(bY);
    double lenZ = glm::length(bZ);
    if (lenX > 1e-10 && lenY > 1e-10 && lenZ > 1e-10) {
        ltc.setBasis(bX / lenX, bY / lenY, bZ / lenZ);
    }

    cell.matrix = ltc.matrix();
    cell.inverseMatrix = ltc.inverseMatrix();
    return cell;
}

struct LightingScenario {
    std::string id;
    double alpha;    // GGX roughness (physical)
    double ndotv;
    ShadingPoint shading;
    PolygonAreaLight light;
    uint32_t samples;
    std::string notes;
};

// Build a rectangular polygon with 4 vertices
// center: center point, halfW/halfH: half-extents along X and Z axes
PolygonAreaLight makeRect(Vec3 center, double halfW, double halfH,
                          Vec3 normal, bool twoSided = false,
                          Vec3 radiance = Vec3(1,1,1))
{
    // Build a frame for the rectangle
    Vec3 N = glm::normalize(normal);
    Vec3 up(0, 1, 0);
    if (std::abs(glm::dot(N, up)) > 0.99) up = Vec3(1, 0, 0);
    Vec3 right = glm::normalize(glm::cross(N, up));
    // Actually align with XZ plane for simplicity when normal is -Z (downward)
    // Use world-space X and Y axes for the rectangle if normal is -Z
    Vec3 axisA, axisB;
    if (std::abs(N.z) > 0.99) {
        axisA = Vec3(1, 0, 0);
        axisB = Vec3(0, 1, 0);
    } else {
        axisA = right;
        axisB = glm::normalize(glm::cross(axisA, N));
    }

    PolygonAreaLight light;
    light.radiance = radiance;
    light.twoSided = twoSided;
    // CCW winding when viewed from front (along -normal direction, toward shading point)
    // This matches the LTC integrator convention (CCW from +z in transformed space)
    light.vertices = {
        center - axisA * halfW - axisB * halfH,
        center + axisA * halfW - axisB * halfH,
        center + axisA * halfW + axisB * halfH,
        center - axisA * halfW + axisB * halfH,
    };
    return light;
}

std::vector<LightingScenario> buildBuiltinScenarios(uint32_t baseSamples)
{
    std::vector<LightingScenario> scenarios;

    // Shading point at origin, normal pointing up (+Z), view pointing up (+Z)
    ShadingPoint sp0;
    sp0.position = Vec3(0, 0, 0);
    sp0.normal   = Vec3(0, 0, 1);
    sp0.view     = Vec3(0, 0, 1);

    // 1. large-frontal-rectangle: 2×2 rect at z=2, alpha=0.5, ndotv=1.0
    {
        LightingScenario s;
        s.id = "large-frontal-rectangle";
        s.alpha = 0.5;
        s.ndotv = 1.0;
        s.shading = sp0;
        s.shading.view = Vec3(0, 0, 1);
        s.light = makeRect(Vec3(0, 0, 2), 1.0, 1.0, Vec3(0, 0, -1));
        s.samples = baseSamples;
        scenarios.push_back(s);
    }

    // 2. small-frontal-rectangle: 0.1×0.1 rect at z=5, alpha=0.5, ndotv=1.0
    {
        LightingScenario s;
        s.id = "small-frontal-rectangle";
        s.alpha = 0.5;
        s.ndotv = 1.0;
        s.shading = sp0;
        s.light = makeRect(Vec3(0, 0, 5), 0.05, 0.05, Vec3(0, 0, -1));
        s.samples = baseSamples;
        scenarios.push_back(s);
    }

    // 3. tilted-rectangle: 1×1 rect rotated 45° about x-axis, alpha=0.3
    {
        LightingScenario s;
        s.id = "tilted-rectangle";
        s.alpha = 0.3;
        s.ndotv = 0.8;
        s.shading = sp0;
        double sinv = std::sqrt(1.0 - 0.8 * 0.8);
        s.shading.view = Vec3(sinv, 0, 0.8);
        // Rect tilted 45 degrees: vertices defined explicitly
        double h = std::sqrt(2.0) * 0.5;  // 1/sqrt(2) for half-size
        s.light.radiance = Vec3(1,1,1);
        s.light.twoSided = true;  // two-sided so both integrators work
        s.light.vertices = {
            Vec3(-0.5, -h, 1.5 + h),
            Vec3( 0.5, -h, 1.5 + h),
            Vec3( 0.5,  h, 1.5 - h),
            Vec3(-0.5,  h, 1.5 - h),
        };
        s.samples = baseSamples;
        scenarios.push_back(s);
    }

    // 4. grazing-rectangle: 1×1 rect at z=2 but ndotv=0.1 (grazing view)
    {
        LightingScenario s;
        s.id = "grazing-rectangle";
        s.alpha = 0.5;
        s.ndotv = 0.1;
        s.shading = sp0;
        double sinv = std::sqrt(1.0 - 0.1 * 0.1);
        s.shading.view = Vec3(sinv, 0, 0.1);
        s.light = makeRect(Vec3(0, 0, 2), 0.5, 0.5, Vec3(0, 0, -1));
        s.samples = baseSamples;
        scenarios.push_back(s);
    }

    // 5. below-horizon: 1×1 rect at z=-2 (fully below horizon → 0)
    {
        LightingScenario s;
        s.id = "below-horizon";
        s.alpha = 0.5;
        s.ndotv = 1.0;
        s.shading = sp0;
        s.light = makeRect(Vec3(0, 0, -2), 0.5, 0.5, Vec3(0, 0, 1));
        s.samples = baseSamples;
        s.notes = "fully below horizon, expect zero";
        scenarios.push_back(s);
    }

    // 6. near-normal-view: ndotv=0.99, low alpha=0.1
    {
        LightingScenario s;
        s.id = "near-normal-view";
        s.alpha = 0.1;
        s.ndotv = 0.99;
        s.shading = sp0;
        double sinv = std::sqrt(1.0 - 0.99 * 0.99);
        s.shading.view = Vec3(sinv, 0, 0.99);
        s.light = makeRect(Vec3(0, 0, 2), 1.0, 1.0, Vec3(0, 0, -1));
        s.samples = baseSamples;
        scenarios.push_back(s);
    }

    // 7. high-roughness: alpha=0.9, 2×2 frontal rect
    {
        LightingScenario s;
        s.id = "high-roughness";
        s.alpha = 0.9;
        s.ndotv = 0.7;
        s.shading = sp0;
        double sinv = std::sqrt(1.0 - 0.7 * 0.7);
        s.shading.view = Vec3(sinv, 0, 0.7);
        s.light = makeRect(Vec3(0, 0, 2), 1.0, 1.0, Vec3(0, 0, -1));
        s.samples = baseSamples;
        scenarios.push_back(s);
    }

    // 8. low-roughness: alpha=0.05, 2×2 frontal rect
    {
        LightingScenario s;
        s.id = "low-roughness";
        s.alpha = 0.05;
        s.ndotv = 0.7;
        s.shading = sp0;
        double sinv = std::sqrt(1.0 - 0.7 * 0.7);
        s.shading.view = Vec3(sinv, 0, 0.7);
        s.light = makeRect(Vec3(0, 0, 2), 1.0, 1.0, Vec3(0, 0, -1));
        s.samples = baseSamples;
        scenarios.push_back(s);
    }

    return scenarios;
}

} // anonymous namespace

std::vector<LightingScenarioResult> ValidationRunner::runLightingValidation(
    const LUTData& lut,
    const ValidationConfig& config) const
{
    std::vector<LightingScenarioResult> results;

    if (lut.width() == 0 || lut.height() == 0) {
        return results;
    }

    auto scenarios = buildBuiltinScenarios(config.lightingSamples);

    GGX brdf;
    NumericalIntegrator numericalIntegrator;
    LTCPolygonIntegrator ltcIntegrator;

    for (const auto& scenario : scenarios) {
        LightingScenarioResult res;
        res.id = scenario.id;
        res.notes = scenario.notes;

        // Look up the LUT cell for this alpha/ndotv
        LUTCell cell = interpolateLUTCell(lut, scenario.alpha, scenario.ndotv);

        // Build BRDFContext
        double sinv = std::sqrt(std::max(0.0, 1.0 - scenario.ndotv * scenario.ndotv));
        Vec3 viewDir(sinv, 0, scenario.ndotv);
        viewDir = glm::normalize(viewDir);
        BRDFContext ctx{viewDir, Scalar(scenario.alpha)};

        // Ground truth via numerical integration
        NumericalIntegrationConfig numCfg;
        numCfg.sampleCount = scenario.samples;
        NumericalIntegrationResult numResult = numericalIntegrator.integrate(
            brdf, ctx, scenario.shading, scenario.light, numCfg);

        // LTC analytical result
        LTCIntegrationResult ltcResult = ltcIntegrator.evaluate(
            cell, scenario.shading, scenario.light);

        // Use luminance (or just R channel) as monochrome comparison
        // Using simple average of RGB
        double gtLum = (numResult.radiance.x + numResult.radiance.y + numResult.radiance.z) / 3.0;
        double ltcLum = (ltcResult.radiance.x + ltcResult.radiance.y + ltcResult.radiance.z) / 3.0;

        res.groundTruth = gtLum;
        res.ltcResult = ltcLum;
        res.absError = std::abs(ltcLum - gtLum);
        res.groundTruthConverged = numResult.converged;

        // Relative error with floor
        double denom = std::max(std::abs(gtLum), config.thresholds.lightingAbsolute);
        res.relError = res.absError / denom;

        results.push_back(res);
    }

    return results;
}

} // namespace ltc
