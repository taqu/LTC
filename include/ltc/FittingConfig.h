#pragma once
#include <cstdint>

namespace ltc {

// Which algorithm variant to use.
enum class FittingProfile
{
    // Reproduces the original ltc_code fitting algorithm as closely as
    // practical.  Uses 32×32 samples, cubic MIS error, Nelder–Mead with
    // step=0.05, tolerance=1e-5, max 100 iterations.
    Reference,

    // Higher-quality mode with configurable sample counts and iteration
    // limits.  Uses the same algorithm family.
    Quality
};

struct FittingConfig
{
    FittingProfile profile        = FittingProfile::Reference;

    // Sample grid resolution (N): N×N samples are drawn per evaluation.
    uint32_t sampleResolution     = 32;

    // Nelder–Mead maximum iteration count.
    uint32_t maxIterations        = 100;

    // Initial simplex step size (perturbation applied to each parameter).
    double   simplexStep          = 0.05;

    // Nelder–Mead convergence tolerance on the objective range.
    double   tolerance            = 1e-5;

    // Minimum positive value clamped onto m11 and m22 to prevent singular
    // matrices.  This is NOT the GGX roughness floor.
    double   minScale             = 1e-7;

    // Use MIS (Multiple Importance Sampling) in the error function.
    // The reference always uses MIS; set false only for debugging.
    bool     useMIS               = true;
};

} // namespace ltc
