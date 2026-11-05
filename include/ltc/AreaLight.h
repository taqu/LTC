#pragma once
#include "Math.h"
#include <string>
#include <vector>

namespace ltc {

struct PolygonAreaLight {
    std::vector<Vec3> vertices;  // CCW winding when viewed from front
    Vec3 radiance{1.0, 1.0, 1.0};
    bool twoSided = false;
};

struct ShadingPoint {
    Vec3 position{0, 0, 0};
    Vec3 normal{0, 0, 1};
    Vec3 view{0, 0, 1};  // unit vector toward viewer
};

// Validate polygon: >= 3 vertices, all finite, non-degenerate
// Returns empty string on success, error description on failure
std::string validatePolygon(const PolygonAreaLight& light);

// Build orthonormal TBN frame from normal and view
// T = normalize(V - N*dot(V,N))
// B = cross(N, T)
// returns [T, B, N] as columns of a Mat3
Mat3 buildShadingFrame(const Vec3& N, const Vec3& V);

} // namespace ltc
