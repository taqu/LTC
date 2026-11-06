#pragma once
#include "FittingResult.h"
#include <cstdint>
#include <vector>

namespace ltc {

enum class InitialGuessType
{
    NeighborCell,   // warm start from a neighboring fitted cell
    Identity,       // m11=1, m22=1, m13=0
    PreviousBest,   // best candidate from a previous stage
    PerturbedBest   // deterministic scale perturbations of best
};

struct InitialGuessCandidate
{
    LTCInitialGuess  guess;
    InitialGuessType type;
};

// Generates a deterministic list of initial guesses for multi-start optimization.
// No random state is used; all perturbation factors are fixed constants.
class InitialGuessStrategy
{
public:
    std::vector<InitialGuessCandidate> generateCandidates(
        const LTCInitialGuess& neighborGuess,
        const LTCInitialGuess* previousBest,
        uint32_t               maxCandidates) const;
};

} // namespace ltc
