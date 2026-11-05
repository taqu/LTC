#pragma once
#include "BRDFCapabilities.h"
#include "Math.h"
#include <string_view>

namespace ltc {

// Input state for a single BRDF evaluation or sample.
// Coordinate system: N = (0,0,1), z-up right-handed.
// Both view and light directions point away from the surface.
struct BRDFContext {
    Vec3   view;  // unit outgoing view direction, z > 0 expected
    Scalar alpha; // physical roughness (not perceptual); alpha = roughness^2
};

// Result of a BRDF evaluation at a given light direction.
struct BRDFEvaluation {
    // Cosine-weighted BRDF value: f_r(V,L) * max(0, N·L).
    // Does NOT include the importance-sampling PDF.
    Scalar value;

    // Solid-angle probability density for the sampled direction.
    // Units: 1/sr.  Zero for invalid or below-hemisphere directions.
    Scalar pdf;
};

// Abstract interface for isotropic scalar BRDFs.
// All methods are stateless with respect to the BRDF instance; they may be
// called concurrently.
class IBRDF {
public:
    virtual ~IBRDF() = default;

    virtual std::string_view name() const = 0;

    // Evaluate the cosine-weighted BRDF and its sampling PDF.
    // light must be a unit direction; invalid inputs return zero value and pdf.
    virtual BRDFEvaluation evaluate(
        const BRDFContext& ctx,
        const Vec3& light) const = 0;

    // Sample a light direction proportional to the BRDF * cosine.
    // u must be in [0, 1)^2.  Returns a unit direction (may be below the
    // hemisphere for some distributions).
    virtual Vec3 sample(
        const BRDFContext& ctx,
        const Vec2& u) const = 0;

    // Return static capability flags for this BRDF.
    virtual BRDFCapabilities capabilities() const = 0;

    // Map perceptualRoughness (from ParameterGrid, in [0,1]) to ctx.alpha.
    // GGX/Beckmann: return max(r*r, 1e-5)
    // DisneyDiffuse: return r  (use directly)
    virtual double contextAlpha(double perceptualRoughness) const = 0;
};

} // namespace ltc
