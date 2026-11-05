#pragma once
#include "LUT.h"
#include <cstdint>

namespace ltc {

struct ValidationReport
{
    uint32_t totalCells       = 0;
    uint32_t completedCells   = 0;
    uint32_t convergedCells   = 0;
    uint32_t maxIterCells     = 0;
    uint32_t failedCells      = 0;

    bool     isComplete       = false; // all cells completed

    // Coefficient quality
    bool     allFinite        = true;
    uint32_t nonFiniteCount   = 0;
    bool     allInvertible    = true;
    uint32_t nonInvertibleCount = 0;
    bool     allPositiveMagnitude = true;
    uint32_t nonPositiveMagnitudeCount = 0;

    // Error statistics over completed cells with finite error
    uint32_t finiteErrorCount = 0;
    double   minFinalError    = 0.0;
    double   maxFinalError    = 0.0;
    double   meanFinalError   = 0.0;
    double   rmsFinalError    = 0.0;

    // Continuity: maximum absolute difference between adjacent cells
    double   maxM11Jump       = 0.0;
    double   maxM22Jump       = 0.0;
    double   maxM13Jump       = 0.0;
    double   maxMagnitudeJump = 0.0;
};

// Stateless validator for a completed LUTData.
class LUTValidator
{
public:
    ValidationReport validate(const LUTData& lut) const;
};

} // namespace ltc
