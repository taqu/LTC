#pragma once
#include "Math.h"
#include <cstdint>

namespace ltc {

enum class FitStatus
{
    Converged,          // Nelder–Mead converged within tolerance
    MaxIterations,      // Reached iteration limit; best-effort result returned
    ZeroEnergy,         // BRDF integrated to essentially zero; no fitting done
    InvalidInput,       // Context or config validation failed
    NumericalFailure    // Optimizer encountered a non-finite objective
};

// LTC matrix parameters for one fitted cell.
struct LTCParameters
{
    double m11 = 1.0;
    double m22 = 1.0;
    double m13 = 0.0;

    // Orthonormal basis stored as column matrix: columns = X, Y, Z.
    Mat3   basis{Scalar(1)}; // identity basis by default
};

// Initial guess passed to the fitter.
struct LTCInitialGuess
{
    double m11 = 1.0;
    double m22 = 1.0;
    double m13 = 0.0;
};

// Full result of a single-point LTC fitting operation.
struct FittingResult
{
    LTCParameters parameters;

    // Hemispherical integral of f_r(V,L)*max(0,N·L).
    double   magnitude            = 0.0;

    // Schlick Fresnel basis coefficient: ∫ (f_target/p) * (1-V·H)^5 / N^2.
    double   fresnelCoefficient   = 0.0;
    bool     hasFresnelCoefficient= false;

    // Normalized average BRDF direction (Y=0 for isotropic).
    Vec3     averageDirection{0.0, 0.0, 1.0};

    // MIS error at the initial and final LTC parameters.
    double   initialError         = 0.0;
    double   finalError           = 0.0;

    uint32_t iterations           = 0;
    uint64_t evaluations          = 0;

    FitStatus status              = FitStatus::InvalidInput;
};

} // namespace ltc
