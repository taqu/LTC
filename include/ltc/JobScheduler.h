#pragma once
#include "LUT.h"
#include "FittingResult.h"
#include <cstdint>
#include <functional>

namespace ltc {

// One unit of fitting work dispatched by the scheduler.
struct SchedulerJob
{
    uint32_t      roughnessIndex;
    uint32_t      viewIndex;
    LTCInitialGuess initial; // initial guess derived from traversal dependencies
};

// Dependency-aware scheduler for reference-compatible LUT generation.
//
// Traversal order: roughness high→low (a = width-1 downto 0),
//                  view 0→height-1 within each roughness row.
//
// Dependencies:
//   (a, 0)  depends on  (a+1, 0)  for a < width-1   [normal-incidence init]
//   (a, t)  depends on  (a, t-1)  for t > 0          [shape-parameter carry]
//
// The worker callback is called once per cell.  It receives the initial guess
// and must return a fully filled LUTCell.  The scheduler writes the result
// into the supplied LUTData.
//
// threadCount == 0 or 1: fully sequential execution.
// threadCount > 1: pipeline parallel (at most threadCount concurrent rows).
class JobScheduler
{
public:
    using WorkFn = std::function<LUTCell(const SchedulerJob&)>;

    JobScheduler(uint32_t width, uint32_t height, uint32_t threadCount);

    // Run fitting across all cells.  Results are written into `output`.
    // `output` must already be allocated with the same dimensions.
    void run(const WorkFn& worker, LUTData& output) const;

    uint32_t width()       const noexcept { return m_width;       }
    uint32_t height()      const noexcept { return m_height;      }
    uint32_t threadCount() const noexcept { return m_threadCount; }

private:
    void runSequential(const WorkFn& worker, LUTData& output) const;
    void runParallel  (const WorkFn& worker, LUTData& output) const;

    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_threadCount;
};

} // namespace ltc
