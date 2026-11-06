#pragma once
#include "BRDF.h"
#include "AreaLight.h"
#include <cstdint>

namespace ltc {

struct NumericalIntegrationConfig {
    uint32_t sampleCount = 65536;  // per polygon triangle
    uint64_t seed = 0;             // deterministic: 0 means fixed sequence
};

struct NumericalIntegrationResult {
    Vec3 radiance{0,0,0};          // outgoing radiance
    Vec3 variance{0,0,0};          // per-channel variance estimate
    uint32_t actualSamples = 0;
    bool converged = false;
};

// CPU numerical integrator for direct illumination from a polygon emitter.
// Uses stratified area sampling. Does NOT use LTC.
// BRDF must follow the IBRDF interface.
class NumericalIntegrator {
public:
    NumericalIntegrationResult integrate(
        const IBRDF& brdf,
        const BRDFContext& ctx,
        const ShadingPoint& shading,
        const PolygonAreaLight& light,
        const NumericalIntegrationConfig& config) const;
};

} // namespace ltc
