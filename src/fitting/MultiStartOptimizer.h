#pragma once
// Internal header — not part of the public API.
#include "ltc/BRDF.h"
#include "ltc/FittingConfig.h"
#include "ltc/FittingResult.h"
#include "ltc/Math.h"
#include "ltc/QualityFitting.h"

namespace ltc {
namespace detail {

struct CandidateResult
{
    FittingResult result;
    double        validationObjective = 1e30;
    bool          valid               = false;
};

// Optimize one LTC fitting candidate, with optional log-space scale parameters.
// Returns the best fitting result found and its independent validation objective.
//
// stageConfig.sampleResolution is used for the optimization objective.
// qcfg.validationSampleResolution is used for the independent validation.
CandidateResult runCandidateOptimization(
    const IBRDF&                brdf,
    const BRDFContext&          ctx,
    double                      magnitude,
    const Vec3&                 basisX,
    const Vec3&                 basisY,
    const Vec3&                 basisZ,
    bool                        normalIncidence,
    const FittingConfig&        stageConfig,
    const QualityFittingConfig& qcfg,
    const LTCInitialGuess&      initial);

} // namespace detail
} // namespace ltc
