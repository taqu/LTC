#pragma once
#include "BRDF.h"
#include "BRDFIntegration.h"
#include "FittingConfig.h"
#include "FittingResult.h"
#include <cstdint>

namespace ltc {

enum class QualityFitStatus
{
    Converged,
    ValidationTargetReached,
    MaximumIterations,
    MaximumEvaluations,
    MaximumSamplingResolution,
    Stagnated,
    InvalidMatrix,
    NumericalFailure
};

struct QualityFittingConfig
{
    uint32_t initialSampleResolution    = 32;
    uint32_t maximumSampleResolution    = 128;

    uint32_t maximumIterations          = 300;
    uint32_t maximumRestarts            = 4;

    bool     adaptiveSampling           = true;
    bool     multiStart                 = true;
    bool     logSpaceScales             = true;

    double   convergenceTolerance       = 1e-7;
    double   minimumScale               = 1e-7;

    uint32_t validationSampleResolution = 128;
    double   stagnationThreshold        = 1e-10;
    uint32_t maximumTotalEvaluations    = 50000;
};

struct QualityFittingDiagnostics
{
    double   initialObjective       = 0.0;
    double   finalObjective         = 0.0;
    double   validationObjective    = 0.0;
    uint32_t totalIterations        = 0;
    uint64_t totalEvaluations       = 0;
    uint32_t restartCount           = 0;
    uint32_t finalSampleResolution  = 0;
    double   minScale               = 0.0;
    double   maxScale               = 0.0;
    double   matrixConditionNumber  = 0.0;
    QualityFitStatus status         = QualityFitStatus::NumericalFailure;
};

struct QualityFittingResult
{
    FittingResult             base;
    QualityFittingDiagnostics diagnostics;
};

// Quality Mode fitting engine.
//
// Extends the reference LTC fitting with:
//   - Staged adaptive sample resolution (32→64→128)
//   - Multiple initial guesses (multi-start)
//   - Optional log-space scale parameterization
//   - Independent validation objective for candidate selection
//
// Reference Mode is unaffected; Quality features activate only when
// QualityFittingConfig options are enabled.
class QualityFitter
{
public:
    QualityFittingResult fit(
        const IBRDF&                 brdf,
        const BRDFContext&           context,
        const BRDFIntegrationResult& precomputed,
        const FittingConfig&         baseConfig,
        const QualityFittingConfig&  qualityConfig,
        const LTCInitialGuess&       initial = {}) const;
};

} // namespace ltc
