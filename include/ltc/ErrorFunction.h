#pragma once
#include "BRDF.h"
#include "LTC.h"
#include "FittingConfig.h"

namespace ltc {

// MIS-based cubic error function comparing a target BRDF against a candidate
// LTC distribution.
//
// Error = (1/N^2) * Σ_i [
//     |f_B(L_i^L) - f_L(L_i^L)|^3 / (p_B(L_i^L) + p_L(L_i^L))
//   + |f_B(L_i^B) - f_L(L_i^B)|^3 / (p_B(L_i^B) + p_L(L_i^B))
// ]
//
// f_B  = cosine-weighted BRDF  (brdf.evaluate().value)
// f_L  = amplitude-scaled LTC  (ltc.evaluate())
// p_B  = BRDF solid-angle PDF  (brdf.evaluate().pdf)
// p_L  = normalized LTC PDF    (ltc.pdf())
// L^L  = direction sampled from LTC
// L^B  = direction sampled from BRDF
class LTCErrorFunction
{
public:
    double evaluate(
        const IBRDF&         brdf,
        const BRDFContext&   context,
        const LTC&           ltc,
        const FittingConfig& config) const;
};

} // namespace ltc
