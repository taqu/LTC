#pragma once
#include "LUT.h"
#include <cstdint>
#include <vector>

namespace ltc {

struct RefinementCriteria
{
    // Cells with finalError above this threshold are refinement candidates.
    double   maxFinalError          = 0.5;

    // Cells that reached the iteration limit are always candidates.
    bool     refitMaxIterCells      = true;

    // Maximum number of cells to select (0 = no limit).
    uint32_t maxRefinementCells     = 0;

    // Accept a refined cell only when relative improvement is >= this fraction.
    // improvement = (originalError - refinedError) / max(originalError, 1e-15)
    double   improvementThreshold   = 0.05;
};

struct RefinementCandidate
{
    uint32_t roughnessIndex;
    uint32_t viewIndex;
    double   score; // higher = needs more refinement
};

// Identifies poor LUT cells and decides whether a refined result should replace
// the original.  All decisions use the finalError stored in LUTCell; no BRDF
// evaluation is performed here.
class RefinementPolicy
{
public:
    explicit RefinementPolicy(const RefinementCriteria& criteria);

    // Returns candidates ranked by descending score.
    std::vector<RefinementCandidate> selectCandidates(const LUTData& lut) const;

    // Returns true when the refined result should replace the original.
    // originalError / refinedError are independent validation objectives from the
    // caller (not the per-cell finalError stored in LUTCell).
    bool shouldAccept(
        const LUTCell& original,
        const LUTCell& refined,
        double         originalValidationError,
        double         refinedValidationError) const;

private:
    RefinementCriteria m_criteria;
};

} // namespace ltc
