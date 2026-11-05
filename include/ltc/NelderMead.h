#pragma once
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace ltc {

enum class OptimizationStatus
{
    Converged,            // |f_best - f_worst| < tolerance
    MaxIterations,        // Reached iteration limit; best candidate returned
    InvalidInitialPoint,  // Initial objective is non-finite
    InvalidObjective,     // All evaluations non-finite
    NumericalFailure      // Internal numerical issue
};

struct OptimizationResult
{
    std::vector<double>  parameters;      // Best parameters found
    double               objectiveValue   = 0.0;
    uint32_t             iterations       = 0;
    uint64_t             evaluations      = 0;
    OptimizationStatus   status           = OptimizationStatus::NumericalFailure;
};

// Nelder–Mead downhill simplex optimizer.
//
// Parameterization (reference-compatible):
//   alpha = 1.0  (reflection)
//   gamma = 2.0  (expansion)
//   rho   = 0.5  (contraction)
//   sigma = 0.5  (shrink)
//
// The optimizer does not depend on BRDF or LTC-specific types.
class NelderMead
{
public:
    using Objective = std::function<double(std::span<const double>)>;

    // Minimize the objective starting from `initial` with the given step,
    // tolerance, and iteration limit.
    // Returns the best candidate found together with diagnostic information.
    OptimizationResult minimize(
        const Objective&          objective,
        std::span<const double>   initial,
        double                    initialStep,
        double                    tolerance,
        uint32_t                  maxIterations) const;
};

} // namespace ltc
