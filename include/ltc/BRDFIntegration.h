#pragma once
#include "BRDF.h"
#include "FittingConfig.h"

namespace ltc {

// Result of hemispherical BRDF integration.
struct BRDFIntegrationResult
{
    // ∫ f_target(V,L) dω  where f_target = f_r(V,L)*max(0,N·L).
    double magnitude              = 0.0;

    // Schlick Fresnel coefficient ∫ (f_target/p) * (1-V·H)^5 / N^2.
    double fresnelCoefficient     = 0.0;
    bool   hasFresnelCoefficient  = false;

    // Normalized, Y-zeroed average BRDF direction.
    Vec3   averageDirection{0.0, 0.0, 1.0};

    // True when the weighted direction sum was large enough to normalize.
    bool   hasValidDirection      = false;
};

// Estimates BRDF hemispherical integrals using BRDF importance sampling over a
// deterministic N×N midpoint grid.
class BRDFIntegrator
{
public:
    BRDFIntegrationResult integrate(
        const IBRDF&         brdf,
        const BRDFContext&   context,
        const FittingConfig& config) const;
};

} // namespace ltc
