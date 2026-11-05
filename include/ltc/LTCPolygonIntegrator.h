#pragma once
#include "AreaLight.h"
#include "LUT.h"
#include <cstdint>

namespace ltc {

enum class LTCIntegrationMode {
    ExactPolygon,      // mathematically accurate spherical polygon + horizon clip
};

struct LTCIntegrationResult {
    Vec3 radiance{0,0,0};
    bool clipped = false;    // true if polygon was partially/fully clipped
    bool degenerate = false; // true if result is unreliable
};

// Analytical LTC polygon integration.
// Uses the inverse LTC matrix to transform the polygon, then integrates
// the spherical polygon against the cosine distribution.
class LTCPolygonIntegrator {
public:
    // Evaluate using explicit LTC parameters (full-precision LUT cell data)
    LTCIntegrationResult evaluate(
        const LUTCell& cell,
        const ShadingPoint& shading,
        const PolygonAreaLight& light,
        LTCIntegrationMode mode = LTCIntegrationMode::ExactPolygon) const;

    // Evaluate using explicit inverse matrix and magnitude
    // (used when reconstructing from DDS)
    LTCIntegrationResult evaluateWithMatrix(
        const Mat3& inverseMatrix,
        double magnitude,
        const ShadingPoint& shading,
        const PolygonAreaLight& light,
        LTCIntegrationMode mode = LTCIntegrationMode::ExactPolygon) const;
};

} // namespace ltc
