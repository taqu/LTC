#include "ltc/AreaLight.h"
#include <cmath>
#include <sstream>

namespace ltc {

std::string validatePolygon(const PolygonAreaLight& light)
{
    if (light.vertices.size() < 3) {
        return "polygon must have at least 3 vertices";
    }

    // Check all vertices are finite
    for (size_t i = 0; i < light.vertices.size(); ++i) {
        if (!isFinite(light.vertices[i])) {
            std::ostringstream oss;
            oss << "vertex " << i << " is not finite";
            return oss.str();
        }
    }

    // Check no duplicate consecutive vertices
    for (size_t i = 0; i < light.vertices.size(); ++i) {
        const Vec3& a = light.vertices[i];
        const Vec3& b = light.vertices[(i + 1) % light.vertices.size()];
        Vec3 diff = a - b;
        double dist = glm::length(diff);
        if (dist < 1e-10) {
            std::ostringstream oss;
            oss << "vertex " << i << " and " << ((i + 1) % light.vertices.size())
                << " are duplicate (distance " << dist << ")";
            return oss.str();
        }
    }

    // Check non-zero area by computing cross products over the fan
    double totalArea = 0.0;
    const Vec3& v0 = light.vertices[0];
    for (size_t i = 1; i + 1 < light.vertices.size(); ++i) {
        const Vec3& v1 = light.vertices[i];
        const Vec3& v2 = light.vertices[i + 1];
        Vec3 cross = glm::cross(v1 - v0, v2 - v0);
        totalArea += glm::length(cross) * 0.5;
    }

    if (totalArea < 1e-20) {
        return "polygon has zero or near-zero area (degenerate)";
    }

    return "";  // success
}

Mat3 buildShadingFrame(const Vec3& N, const Vec3& V)
{
    // T = normalize(V - N*dot(V,N))
    Vec3 T = V - N * glm::dot(V, N);
    double len = glm::length(T);
    if (len < 1e-6) {
        // Fallback: build arbitrary tangent
        // Use a vector not parallel to N
        if (std::abs(N.x) < 0.9) {
            T = glm::normalize(glm::cross(N, Vec3(1, 0, 0)));
        } else {
            T = glm::normalize(glm::cross(N, Vec3(0, 1, 0)));
        }
    } else {
        T = T / len;
    }

    Vec3 B = glm::cross(N, T);

    // GLM Mat3 constructor takes columns
    return Mat3(T, B, N);
}

} // namespace ltc
