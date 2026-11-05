#include "ltc/ErrorFunction.h"
#include "ltc/Sampling.h"
#include <cmath>
#include <algorithm>

namespace ltc {

namespace {

inline Scalar cubicDiff(Scalar a, Scalar b) noexcept
{
    Scalar d = std::abs(a - b);
    return d * d * d;
}

} // anonymous namespace

double LTCErrorFunction::evaluate(
    const IBRDF&         brdf,
    const BRDFContext&   ctx,
    const LTC&           ltc,
    const FittingConfig& config) const
{
    SampleGrid grid(config.sampleResolution);
    uint32_t N = grid.resolution();
    Scalar N2  = static_cast<Scalar>(N) * static_cast<Scalar>(N);

    Scalar sum = Scalar(0);

    for (uint32_t j = 0; j < N; ++j) {
        for (uint32_t i = 0; i < N; ++i) {
            Sample2D s = grid.sample(i, j);

            // ---- Pass 1: sample from LTC ----
            {
                Vec3 L = ltc.sample(Vec2(s.u1, s.u2));

                BRDFEvaluation ev = brdf.evaluate(ctx, L);
                Scalar f_B = std::isfinite(ev.value) ? ev.value : Scalar(0);
                Scalar p_B = std::isfinite(ev.pdf)   ? ev.pdf   : Scalar(0);

                Scalar f_L = ltc.evaluate(L);
                Scalar p_L = ltc.pdf(L);

                Scalar denom = p_B + p_L;
                if (denom > Scalar(0))
                    sum += cubicDiff(f_B, f_L) / denom;
            }

            // ---- Pass 2: sample from BRDF ----
            {
                Vec3 L = brdf.sample(ctx, Vec2(s.u1, s.u2));

                BRDFEvaluation ev = brdf.evaluate(ctx, L);
                Scalar f_B = std::isfinite(ev.value) ? ev.value : Scalar(0);
                Scalar p_B = std::isfinite(ev.pdf)   ? ev.pdf   : Scalar(0);

                Scalar f_L = ltc.evaluate(L);
                Scalar p_L = ltc.pdf(L);

                Scalar denom = p_B + p_L;
                if (denom > Scalar(0))
                    sum += cubicDiff(f_B, f_L) / denom;
            }
        }
    }

    return static_cast<double>(sum / N2);
}

} // namespace ltc
