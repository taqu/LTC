#include "ltc/BRDFIntegration.h"
#include "ltc/Sampling.h"
#include <cmath>
#include <algorithm>

namespace ltc {

BRDFIntegrationResult BRDFIntegrator::integrate(
    const IBRDF&         brdf,
    const BRDFContext&   ctx,
    const FittingConfig& config) const
{
    SampleGrid grid(config.sampleResolution);
    uint32_t N = grid.resolution();
    Scalar N2 = static_cast<Scalar>(N) * static_cast<Scalar>(N);

    Scalar magSum    = Scalar(0);
    Scalar fcSum     = Scalar(0);
    Vec3   dirSum    = Vec3(0);
    bool   fcValid   = false;

    for (uint32_t j = 0; j < N; ++j) {
        for (uint32_t i = 0; i < N; ++i) {
            Sample2D s = grid.sample(i, j);
            Vec3 L = brdf.sample(ctx, Vec2(s.u1, s.u2));

            BRDFEvaluation ev = brdf.evaluate(ctx, L);
            if (ev.pdf <= Scalar(0) || !std::isfinite(ev.pdf))
                continue;
            if (!std::isfinite(ev.value))
                continue;

            Scalar w = ev.value / ev.pdf; // f_target / p_L (importance weight)
            magSum += w;

            // Accumulate weighted direction (zero out Y for isotropic brdf).
            dirSum += w * L;

            // Schlick Fresnel coefficient: weight * (1 - V·H)^5
            Vec3   H    = ctx.view + L;
            Scalar hLen = glm::length(H);
            if (hLen > Scalar(1e-10)) {
                H /= hLen;
                Scalar dotVH    = glm::dot(ctx.view, H);
                Scalar oneMinusDotVH = Scalar(1) - dotVH;
                // Clamp to [0,1] for numerical safety.
                oneMinusDotVH = std::clamp(oneMinusDotVH, Scalar(0), Scalar(1));
                Scalar schlick = oneMinusDotVH * oneMinusDotVH * oneMinusDotVH
                               * oneMinusDotVH * oneMinusDotVH;
                fcSum  += w * schlick;
                fcValid = true;
            }
        }
    }

    BRDFIntegrationResult result;
    result.magnitude             = magSum / N2;
    result.fresnelCoefficient    = fcSum  / N2;
    result.hasFresnelCoefficient = fcValid;

    // For isotropic BRDFs, zero out Y before normalizing the average direction.
    Vec3 dirXZ(dirSum.x, Scalar(0), dirSum.z);
    Scalar dirLen = glm::length(dirXZ);
    if (dirLen > Scalar(1e-10)) {
        result.averageDirection  = dirXZ / dirLen;
        result.hasValidDirection = true;
    } else {
        result.averageDirection  = Vec3(0, 0, 1);
        result.hasValidDirection = false;
    }

    return result;
}

} // namespace ltc
