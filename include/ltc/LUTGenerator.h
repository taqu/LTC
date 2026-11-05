#pragma once
#include "BRDF.h"
#include "GenerationConfig.h"
#include "GenerationProgress.h"
#include "LUT.h"
#include "LUTValidation.h"

namespace ltc {

// High-level LTC LUT generation engine.
//
// Responsibilities:
//   1. Validate GenerationConfig.
//   2. Create the ParameterGrid.
//   3. Allocate LUTData.
//   4. Load checkpoint if resuming.
//   5. Schedule cell fitting via JobScheduler.
//   6. Provide reference-compatible initial guesses.
//   7. Store full fitting results.
//   8. Save checkpoints at configured intervals.
//   9. Return a fully populated LUTData.
//
// The generator does NOT:
//   - Write DDS files.
//   - Quantize to FP16.
//   - Perform BRDF-specific equations.
//   - Implement numerical optimization internally.
class LUTGenerator
{
public:
    // Generate a complete LUT.
    // progress may be nullptr.
    // Throws std::invalid_argument on invalid config.
    // Throws CheckpointError on checkpoint I/O failures.
    LUTData generate(
        const IBRDF&         brdf,
        const GenerationConfig& config,
        GenerationProgress*  progress = nullptr) const;
};

} // namespace ltc
