#pragma once
#include "FittingConfig.h"
#include <cstdint>
#include <string>

namespace ltc {

struct GenerationConfig
{
    // LUT dimensions.  Both must be >= 2.
    uint32_t width  = 64;
    uint32_t height = 64;

    // Worker thread count.  0 or 1 means sequential execution.
    // "auto" is handled at CLI level by passing hardware_concurrency().
    uint32_t threads = 1;

    // Fitting parameters.
    FittingConfig fitting;

    // Checkpoint settings.
    std::string checkpointPath;
    bool        enableCheckpoint      = false;
    uint32_t    checkpointInterval    = 64; // save every N completed cells
    bool        resumeFromCheckpoint  = false;
};

} // namespace ltc
