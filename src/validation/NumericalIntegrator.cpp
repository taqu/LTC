#include "ltc/NumericalIntegrator.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace ltc {

namespace {

// Triangle area
double triangleArea(const Vec3& A, const Vec3& B, const Vec3& C)
{
    Vec3 cross = glm::cross(B - A, C - A);
    return glm::length(cross) * 0.5;
}

// Triangle normal (CCW → points toward viewer by convention)
Vec3 triangleNormal(const Vec3& A, const Vec3& B, const Vec3& C)
{
    Vec3 cross = glm::cross(B - A, C - A);
    double len = glm::length(cross);
    if (len < 1e-15) return Vec3(0, 0, 1);
    return cross / len;
}

// Sample a point uniformly within a triangle using the standard method
// r1, r2 in [0,1)
Vec3 sampleTriangle(const Vec3& A, const Vec3& B, const Vec3& C, double r1, double r2)
{
    double u1 = std::sqrt(r1);
    double u2 = r2;
    double a = 1.0 - u1;
    double b = u1 * (1.0 - u2);
    double c = u1 * u2;
    return A * a + B * b + C * c;
}

// Simple deterministic low-discrepancy sample using Van der Corput sequence
double vanDerCorput(uint32_t n, uint32_t base)
{
    double result = 0.0;
    double denom = 1.0;
    while (n > 0) {
        denom *= base;
        result += (n % base) / denom;
        n /= base;
    }
    return result;
}

} // anonymous namespace

NumericalIntegrationResult NumericalIntegrator::integrate(
    const IBRDF& brdf,
    const BRDFContext& ctx,
    const ShadingPoint& shading,
    const PolygonAreaLight& light,
    const NumericalIntegrationConfig& config) const
{
    NumericalIntegrationResult result;

    const auto& verts = light.vertices;
    if (verts.size() < 3) {
        return result;
    }

    // Triangulate polygon (fan from vertex 0)
    int numTriangles = (int)verts.size() - 2;
    if (numTriangles <= 0) return result;

    // Compute area of each triangle and total
    std::vector<double> triAreas(numTriangles);
    double totalArea = 0.0;
    for (int i = 0; i < numTriangles; ++i) {
        const Vec3& A = verts[0];
        const Vec3& B = verts[i + 1];
        const Vec3& C = verts[i + 2];
        triAreas[i] = triangleArea(A, B, C);
        totalArea += triAreas[i];
    }

    if (totalArea < 1e-20) {
        return result;
    }

    // Distribute samples proportionally to triangle area
    uint32_t totalSamples = config.sampleCount;
    std::vector<uint32_t> triSamples(numTriangles);
    uint32_t assignedSamples = 0;
    for (int i = 0; i < numTriangles; ++i) {
        uint32_t s = (uint32_t)(totalSamples * triAreas[i] / totalArea);
        triSamples[i] = std::max(s, 1u);
        assignedSamples += triSamples[i];
    }
    // Adjust last triangle to hit exactly totalSamples
    if (assignedSamples < totalSamples) {
        triSamples[numTriangles - 1] += (totalSamples - assignedSamples);
    } else if (assignedSamples > totalSamples) {
        // Reduce from last triangle, keeping at least 1
        uint32_t excess = assignedSamples - totalSamples;
        uint32_t last = triSamples[numTriangles - 1];
        triSamples[numTriangles - 1] = (last > excess + 1) ? (last - excess) : 1u;
    }

    Vec3 sumRadiance{0, 0, 0};
    Vec3 sumRadiance2{0, 0, 0};
    uint32_t actualSamples = 0;

    uint32_t sampleOffset = 0;  // global sample index for deterministic sequences

    for (int ti = 0; ti < numTriangles; ++ti) {
        const Vec3& A = verts[0];
        const Vec3& B = verts[ti + 1];
        const Vec3& C = verts[ti + 2];

        Vec3 triNorm = triangleNormal(A, B, C);
        uint32_t ns = triSamples[ti];
        (void)triAreas[ti]; // area used only via totalArea/sample distribution

        for (uint32_t si = 0; si < ns; ++si) {
            // Deterministic quasi-random samples
            uint32_t idx = sampleOffset + si;
            double r1 = vanDerCorput(idx + 1, 2);
            double r2 = vanDerCorput(idx + 1, 3);

            // Sample point on triangle
            Vec3 X = sampleTriangle(A, B, C, r1, r2);

            // Direction from shading point to sample
            Vec3 dir = X - shading.position;
            double dist2 = glm::dot(dir, dir);
            if (dist2 < 1e-20) continue;

            double dist = std::sqrt(dist2);
            Vec3 L = dir / dist;

            // Cosine at shading point
            double ndotl = glm::dot(shading.normal, L);
            if (ndotl <= 0.0) continue;

            // Emission cosine (geometric factor at light)
            double cosEmit;
            if (light.twoSided) {
                cosEmit = std::abs(glm::dot(triNorm, -L));
            } else {
                cosEmit = glm::dot(triNorm, -L);
                if (cosEmit <= 0.0) continue;
            }

            // BRDF evaluation (note: ctx has view and alpha already set)
            // brdf.evaluate().value returns f_r(V,L) * max(0, N·L) — already cosine-weighted
            double brdfVal = brdf.evaluate(ctx, L).value;

            // Monte Carlo estimator for direct illumination from area light:
            //   L_o = integral over light { f_r * L_i * ndotl * G } dA
            // where G = cos_emit / dist2 is the geometric term,
            // and brdfVal = f_r * ndotl already includes the ndotl factor.
            // PDF = 1/totalArea (uniform area sampling)
            // Contribution: brdfVal * cos_emit / dist2
            // Final estimator: (totalArea / N) * sum(contribution * radiance)
            double contribution = brdfVal * cosEmit / dist2;

            Vec3 sample = light.radiance * contribution;
            sumRadiance += sample;
            sumRadiance2 += sample * sample;
            ++actualSamples;
        }
        sampleOffset += ns;
    }

    if (actualSamples == 0) {
        result.actualSamples = 0;
        return result;
    }

    // Final estimator: totalArea / actualSamples * sum
    double scale = totalArea / static_cast<double>(actualSamples);
    result.radiance = sumRadiance * scale;

    // Variance estimate
    Vec3 mean = sumRadiance / static_cast<double>(actualSamples);
    Vec3 mean2 = sumRadiance2 / static_cast<double>(actualSamples);
    Vec3 variance;
    variance.x = std::max(0.0, mean2.x - mean.x * mean.x * scale * scale);
    variance.y = std::max(0.0, mean2.y - mean.y * mean.y * scale * scale);
    variance.z = std::max(0.0, mean2.z - mean.z * mean.z * scale * scale);
    result.variance = variance;
    result.actualSamples = actualSamples;
    result.converged = true;
    return result;
}

} // namespace ltc
