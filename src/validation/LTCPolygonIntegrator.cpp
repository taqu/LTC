#include "ltc/LTCPolygonIntegrator.h"
#include "ltc/AreaLight.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace ltc {

namespace {

// Clip polygon vertices (in LTC space) against the z >= 0 hemisphere plane
// using Sutherland-Hodgman
std::vector<Vec3> clipPolygonToHorizon(const std::vector<Vec3>& L)
{
    std::vector<Vec3> out;
    int n = (int)L.size();
    if (n == 0) return out;

    for (int i = 0; i < n; ++i) {
        const Vec3& a = L[i];
        const Vec3& b = L[(i + 1) % n];
        bool aIn = a.z > 0.0;
        bool bIn = b.z > 0.0;

        if (aIn) {
            out.push_back(a);
        }
        if (aIn != bIn) {
            // Compute intersection with z=0 plane
            double denom = a.z - b.z;
            if (std::abs(denom) > 1e-20) {
                double t = a.z / denom;
                Vec3 p = a + t * (b - a);
                p.z = 0.0;  // force exact
                out.push_back(p);
            }
        }
    }
    return out;
}

// Signed edge integral for the spherical polygon formula
// v1 and v2 must be on the unit sphere
double integrateEdge(const Vec3& v1, const Vec3& v2)
{
    double cosT = glm::dot(v1, v2);
    cosT = std::max(-1.0, std::min(1.0, cosT));
    double theta = std::acos(cosT);
    Vec3 crossVec = glm::cross(v1, v2);
    double len = glm::length(crossVec);
    if (len < 1e-10) return 0.0;
    return theta * (crossVec / len).z;
}

LTCIntegrationResult integratePolygon(
    const Mat3& inverseMatrix,
    double magnitude,
    const ShadingPoint& shading,
    const PolygonAreaLight& light)
{
    LTCIntegrationResult result;

    const auto& verts = light.vertices;
    if (verts.size() < 3) {
        result.degenerate = true;
        return result;
    }

    // Build local shading frame from N and V
    Mat3 frame = buildShadingFrame(shading.normal, shading.view);

    // Combined matrix: transforms world-space directions to LTC cosine space
    // Minv_combined = inverseMatrix * transpose(frame)
    Mat3 frameT = glm::transpose(frame);
    Mat3 Minv = inverseMatrix * frameT;

    // Transform polygon vertices to LTC space (unnormalized)
    std::vector<Vec3> L;
    L.reserve(verts.size());
    for (const auto& v : verts) {
        Vec3 d = v - shading.position;
        L.push_back(Minv * d);
    }

    // Horizon clipping (clip against z >= 0 plane)
    std::vector<Vec3> clipped = clipPolygonToHorizon(L);
    if (clipped.empty()) {
        // Fully below horizon
        return result;
    }
    result.clipped = (clipped.size() != L.size());

    // Normalize all vertices to unit sphere
    std::vector<Vec3> normalized;
    normalized.reserve(clipped.size());
    for (const auto& v : clipped) {
        double len = glm::length(v);
        if (len < 1e-10) {
            result.degenerate = true;
            return result;
        }
        normalized.push_back(v / len);
    }

    // Integrate spherical polygon against cosine distribution
    double sum = 0.0;
    int n = (int)normalized.size();
    for (int i = 0; i < n; ++i) {
        const Vec3& a = normalized[i];
        const Vec3& b = normalized[(i + 1) % n];
        sum += integrateEdge(a, b);
    }

    // The integral is sum / (2*pi), giving the irradiance factor
    double irradiance = std::max(0.0, sum) / kTwoPi;

    // Apply magnitude and radiance
    double radiance = magnitude * irradiance;
    result.radiance = light.radiance * radiance;
    return result;
}

} // anonymous namespace

LTCIntegrationResult LTCPolygonIntegrator::evaluate(
    const LUTCell& cell,
    const ShadingPoint& shading,
    const PolygonAreaLight& light,
    LTCIntegrationMode /*mode*/) const
{
    return integratePolygon(cell.inverseMatrix, cell.magnitude, shading, light);
}

LTCIntegrationResult LTCPolygonIntegrator::evaluateWithMatrix(
    const Mat3& inverseMatrix,
    double magnitude,
    const ShadingPoint& shading,
    const PolygonAreaLight& light,
    LTCIntegrationMode /*mode*/) const
{
    return integratePolygon(inverseMatrix, magnitude, shading, light);
}

} // namespace ltc
