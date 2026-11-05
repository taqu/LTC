#pragma once
#include "BRDF.h"

namespace ltc {

// Isotropic Beckmann microfacet reflection BRDF.
//
// NDF: Beckmann distribution
//   D(H; alpha) = exp(-tan^2(theta_h) / alpha^2) / (pi * alpha^2 * cos^4(theta_h))
//
// Geometry: Smith masking-shadowing using the Walter et al. rational approximation
//   a = cosTheta / (alpha * sinTheta)
//   G1 = 1 if a >= 1.6, else (3.535*a + 2.181*a^2) / (1 + 2.276*a + 2.577*a^2)
//   G(V,L) = G1(V) * G1(L)
//
// Fresnel: none (Fresnel = 1)
//
// Evaluation returns: f_r(V,L) * max(0, N·L)
// where f_r(V,L) = D(H)*G(V,L) / (4*(N·V))
//
// Sampling uses Beckmann NDF half-vector sampling.
// PDF is in solid-angle units for the reflected direction L.
class Beckmann final : public IBRDF {
public:
    std::string_view name() const override;

    BRDFEvaluation evaluate(
        const BRDFContext& ctx,
        const Vec3& light) const override;

    Vec3 sample(
        const BRDFContext& ctx,
        const Vec2& u) const override;

    BRDFCapabilities capabilities() const override;
    double contextAlpha(double perceptualRoughness) const override;

    // ---- Exposed sub-components (for testing) ----

    // Beckmann NDF value D(H) for a given roughness and half-vector cosine.
    // Returns 0 when cosH <= 0 or alpha <= 0.
    static Scalar ndf(Scalar alpha, Scalar cosH) noexcept;

    // Smith Beckmann G1 masking term (Walter rational approximation).
    // Returns 0 when cosTheta <= 0.
    static Scalar smithG1(Scalar alpha, Scalar cosTheta, Scalar sinTheta) noexcept;
};

} // namespace ltc
