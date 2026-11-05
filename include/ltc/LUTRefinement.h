#pragma once
#include "BRDF.h"
#include "FittingConfig.h"
#include "GenerationProgress.h"
#include "LUT.h"
#include "QualityFitting.h"
#include "RefinementPolicy.h"

namespace ltc {

struct RefinementResult
{
    uint32_t candidateCount  = 0; // cells selected for refinement
    uint32_t refinedCount    = 0; // cells actually replaced
    uint32_t rejectedCount   = 0; // refined but not accepted (original preserved)
    double   meanImprovement = 0.0;
};

// Runs a Quality Mode post-generation refinement pass.
//
// Identifies poorly fitted cells using RefinementPolicy, refits each with
// QualityFitter, and replaces cells whose independent validation error
// improves by at least RefinementCriteria::improvementThreshold.
//
// Important: refinement treats each cell independently.  Neighbouring cells
// that were seeded from a refined cell during the original sequential pass are
// left unchanged (the original sequential traversal is not re-run).
// Callers that require full sequential consistency should re-generate the LUT.
class LUTRefiner
{
public:
    RefinementResult refine(
        LUTData&                    lut,
        const IBRDF&                brdf,
        const FittingConfig&        baseConfig,
        const QualityFittingConfig& qualityConfig,
        const RefinementCriteria&   criteria,
        GenerationProgress*         progress = nullptr) const;
};

} // namespace ltc
