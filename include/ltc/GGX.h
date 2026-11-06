#pragma once
#include "BRDF.h"

namespace ltc {

// Isotropic GGX microfacet reflection BRDF.
//
// NDF: Trowbridge-Reitz (GGX)
//   D(H) = alpha^2 / (pi * ((N·H)^2*(alpha^2-1)+1)^2)
//
// Geometry: uncorrelated Smith masking-shadowing using the GGX G1 term
//   G1(V) = 2*(N·V) / ((N·V) + sqrt(alpha^2 + (1-alpha^2)*(N·V)^2))
//   G(V,L) = G1(V) * G1(L)
//
// Fresnel: none (Fresnel = 1) — fitting target convention from the reference
//   implementation (selfshadow/ltc_code).
//
// Evaluation returns: f_r(V,L) * max(0, N·L)
// where f_r(V,L) = D(H)*G(V,L) / (4*(N·V))
//
// Sampling uses GGX NDF half-vector sampling.
// PDF is in solid-angle units for the reflected direction L.
class GGX final : public IBRDF {
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

    // GGX NDF value D(H) for a given roughness and half-vector cosine.
    // Returns 0 when cosH <= 0 or alpha <= 0.
    static Scalar ndf(Scalar alpha, Scalar cosH) noexcept;

    // Smith uncorrelated GGX G1 masking term.
    // Returns 0 when cosV <= 0.
    static Scalar smithG1(Scalar alpha, Scalar cosV) noexcept;

    // Solid-angle PDF for a sampled light direction L given view V.
    // Returns 0 for invalid directions.
    static Scalar pdf(Scalar alpha, const Vec3& view, const Vec3& light) noexcept;
};

} // namespace ltc
