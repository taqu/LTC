#include "ltc/RefinementPolicy.h"
#include <algorithm>
#include <cmath>

namespace ltc {

RefinementPolicy::RefinementPolicy(const RefinementCriteria& criteria)
    : m_criteria(criteria)
{}

std::vector<RefinementCandidate> RefinementPolicy::selectCandidates(const LUTData& lut) const
{
    std::vector<RefinementCandidate> candidates;

    uint32_t W = lut.width();
    uint32_t H = lut.height();

    for (uint32_t v = 0; v < H; ++v) {
        for (uint32_t r = 0; r < W; ++r) {
            const LUTCell& cell = lut.at(r, v);
            if (!cell.completed) continue;

            bool isCandidate = false;
            double score     = 0.0;

            // High error.
            if (std::isfinite(cell.finalError) &&
                cell.finalError > m_criteria.maxFinalError)
            {
                isCandidate = true;
                score = std::max(score, cell.finalError);
            }

            // Reached iteration limit.
            if (m_criteria.refitMaxIterCells &&
                cell.status == FitStatus::MaxIterations)
            {
                isCandidate = true;
                score = std::max(score, cell.finalError);
            }

            if (isCandidate)
                candidates.push_back({r, v, score});
        }
    }

    // Sort by descending score (worst cells first).
    std::sort(candidates.begin(), candidates.end(),
              [](const RefinementCandidate& a, const RefinementCandidate& b) {
                  return a.score > b.score;
              });

    if (m_criteria.maxRefinementCells > 0 &&
        candidates.size() > m_criteria.maxRefinementCells)
    {
        candidates.resize(m_criteria.maxRefinementCells);
    }

    return candidates;
}

bool RefinementPolicy::shouldAccept(
    const LUTCell& /*original*/,
    const LUTCell& /*refined*/,
    double         originalValidationError,
    double         refinedValidationError) const
{
    // Reject if refined error is non-finite.
    if (!std::isfinite(refinedValidationError)) return false;

    // Require the original to be finite before comparing.
    if (!std::isfinite(originalValidationError)) {
        // If original was invalid, accept any finite improvement.
        return true;
    }

    double improvement = (originalValidationError - refinedValidationError) /
                          std::max(originalValidationError, 1e-15);
    return improvement >= m_criteria.improvementThreshold;
}

} // namespace ltc
