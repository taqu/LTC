#pragma once
#include "BRDF.h"
#include "LUT.h"
#include "ParameterGrid.h"
#include <cstdint>

namespace ltc {

struct DistributionErrorMetrics {
    double l1Error = 0;         // integral of |f_B - f_L| dω
    double l2Error = 0;         // sqrt(integral of (f_B - f_L)^2 dω)
    double cubicError = 0;      // integral of |f_B - f_L|^3 dω
    double energyError = 0;     // |integral f_B - integral f_L|
    double maxAbsError = 0;
    Vec3 maxErrorDirection{0,0,1};
    uint32_t sampleCount = 0;
};

struct DistributionValidationConfig {
    uint32_t sampleCount = 65536;  // must differ from fitting grid size
};

// Validate a single LUT cell: compare BRDF vs LTC approximation
DistributionErrorMetrics validateDistribution(
    const IBRDF& brdf,
    const LUTCell& cell,
    const LUTParameters& params,
    const DistributionValidationConfig& config);

} // namespace ltc
