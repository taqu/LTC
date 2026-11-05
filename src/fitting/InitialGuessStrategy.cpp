#include "ltc/InitialGuessStrategy.h"

namespace ltc {

std::vector<InitialGuessCandidate> InitialGuessStrategy::generateCandidates(
    const LTCInitialGuess& neighborGuess,
    const LTCInitialGuess* previousBest,
    uint32_t               maxCandidates) const
{
    std::vector<InitialGuessCandidate> candidates;
    candidates.reserve(maxCandidates);

    auto push = [&](const LTCInitialGuess& g, InitialGuessType t) {
        if (candidates.size() < maxCandidates)
            candidates.push_back({g, t});
    };

    // 1. Neighbor cell warm start (always first).
    push(neighborGuess, InitialGuessType::NeighborCell);

    // 2. Identity (useful when neighbor is poor or not available).
    LTCInitialGuess identity{1.0, 1.0, 0.0};
    bool identityDiffers = (identity.m11 != neighborGuess.m11 ||
                             identity.m22 != neighborGuess.m22 ||
                             identity.m13 != neighborGuess.m13);
    if (identityDiffers)
        push(identity, InitialGuessType::Identity);

    // 3. Previous best (if available and distinct from neighbor).
    if (previousBest) {
        bool differs = (previousBest->m11 != neighborGuess.m11 ||
                        previousBest->m22 != neighborGuess.m22 ||
                        previousBest->m13 != neighborGuess.m13);
        if (differs)
            push(*previousBest, InitialGuessType::PreviousBest);
    }

    // 4. Deterministic perturbations of the neighbor guess.
    // Scale factors chosen to cover both larger and smaller lobe widths.
    static constexpr double kFactors[] = {1.5, 0.5, 2.0};
    for (double f : kFactors) {
        LTCInitialGuess p = neighborGuess;
        p.m11 *= f;
        p.m22 *= f;
        push(p, InitialGuessType::PerturbedBest);
    }

    return candidates;
}

} // namespace ltc
