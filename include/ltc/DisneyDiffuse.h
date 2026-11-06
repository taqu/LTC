#pragma once
#include "BRDF.h"

namespace ltc {

// Disney/Burley diffuse BRDF.
//
// fd = (1/pi) * (1 + (FD90-1)*FL) * (1 + (FD90-1)*FV)
// where
//   FD90 = 0.5 + 2 * roughness * cosLH^2
//   FL   = (1 - cosNL)^5
//   FV   = (1 - cosNV)^5
//
// ctx.alpha carries the perceptual roughness directly (no squaring).
//
// Importance sampling: cosine-weighted hemisphere.
// PDF = cosNL / pi.
class DisneyDiffuse final : public IBRDF {
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
};

} // namespace ltc
