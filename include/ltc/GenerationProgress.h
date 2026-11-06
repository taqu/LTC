#pragma once
#include <atomic>
#include <cstdint>
#include <functional>

namespace ltc {

// Progress counters updated atomically by the LUT generator.
// Pass a pointer to LUTGenerator::generate(); nullptr disables progress tracking.
struct GenerationProgress
{
    uint32_t totalCells = 0;

    std::atomic<uint32_t> completedCells{0};
    std::atomic<uint32_t> convergedCells{0};
    std::atomic<uint32_t> maxIterCells{0};
    std::atomic<uint32_t> failedCells{0};
    std::atomic<uint32_t> activeWorkers{0};

    // Called from worker threads after each cell completes.
    // The callback must be thread-safe.
    std::function<void()> onCellComplete;

    // Set to true to request cancellation after the current cell.
    // Worker threads check this flag and stop early if set.
    std::atomic<bool> cancel{false};

    // Non-copyable due to atomics.
    GenerationProgress() = default;
    GenerationProgress(const GenerationProgress&) = delete;
    GenerationProgress& operator=(const GenerationProgress&) = delete;
};

} // namespace ltc
