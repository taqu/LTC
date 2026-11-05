#pragma once
#include "BRDF.h"
#include "BRDFIntegration.h"
#include "FittingConfig.h"
#include "FittingResult.h"

namespace ltc {

// Single-point LTC fitting engine.
//
// Fits a Linearly Transformed Cosine distribution to a target BRDF at one
// (view, alpha) operating point.
//
// The fitter:
//   1. Integrates the BRDF to obtain magnitude, Fresnel coefficient, and
//      average direction.
//   2. Constructs an LTC orientation from the average BRDF direction.
//   3. Runs Nelder–Mead to minimize the MIS error function.
//   4. Returns the fitted LTC parameters and diagnostics.
//
// The fitter does not modify the supplied BRDF.
class LTCFitter
{
public:
    // Standard overload: performs BRDF integration internally.
    FittingResult fit(
        const IBRDF&           brdf,
        const BRDFContext&     context,
        const FittingConfig&   config,
        const LTCInitialGuess& initial = {}) const;

    // Overload for callers that have already integrated the BRDF.
    // Skips the integration step; uses precomputed directly.
    // Useful for LUT generation to avoid re-integrating per initial-guess attempt.
    FittingResult fit(
        const IBRDF&                   brdf,
        const BRDFContext&             context,
        const BRDFIntegrationResult&   precomputed,
        const FittingConfig&           config,
        const LTCInitialGuess&         initial = {}) const;
};

} // namespace ltc
