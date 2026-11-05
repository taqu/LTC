#include "ltc/JobScheduler.h"
#include <future>
#include <semaphore>
#include <thread>
#include <vector>

namespace ltc {

JobScheduler::JobScheduler(uint32_t width, uint32_t height, uint32_t threadCount)
    : m_width(width)
    , m_height(height)
    , m_threadCount(threadCount)
{
}

void JobScheduler::run(const WorkFn& worker, LUTData& output) const
{
    if (m_threadCount <= 1)
        runSequential(worker, output);
    else
        runParallel(worker, output);
}

// ---- Helpers ----

static LTCInitialGuess initialFromCell(const LUTCell& cell)
{
    return {cell.parameters.m11, cell.parameters.m22, cell.parameters.m13};
}

static LTCInitialGuess normalIncidenceInitFromCell(const LUTCell& cell)
{
    // For the t=0 initialization, m13 is reset to 0 (isotropic constraint).
    return {cell.parameters.m11, cell.parameters.m22, 0.0};
}

// ---- Sequential implementation ----

void JobScheduler::runSequential(const WorkFn& worker, LUTData& output) const
{
    for (int a = static_cast<int>(m_width) - 1; a >= 0; --a) {
        uint32_t ua = static_cast<uint32_t>(a);

        // Derive t=0 initial guess.
        LTCInitialGuess rowInit;
        if (ua == m_width - 1) {
            rowInit = {1.0, 1.0, 0.0}; // identity — first row
        } else {
            rowInit = normalIncidenceInitFromCell(output.at(ua + 1, 0));
        }

        for (uint32_t t = 0; t < m_height; ++t) {
            LTCInitialGuess init = (t == 0) ? rowInit
                                            : initialFromCell(output.at(ua, t - 1));
            SchedulerJob job{ua, t, init};
            output.at(ua, t) = worker(job);
        }
    }
}

// ---- Parallel implementation ----
//
// Each roughness row runs as an independent thread.
// A promise/future pair passes the t=0 initial guess from row (a+1) to row (a).
// A counting_semaphore limits concurrency to m_threadCount active rows.

void JobScheduler::runParallel(const WorkFn& worker, LUTData& output) const
{
    const uint32_t W = m_width;
    const uint32_t H = m_height;

    // promises[a] is fulfilled with the initial guess for row a's t=0 cell.
    std::vector<std::promise<LTCInitialGuess>> promises(W);
    std::vector<std::future<LTCInitialGuess>>  futures(W);
    for (uint32_t a = 0; a < W; ++a)
        futures[a] = promises[a].get_future();

    // Pre-fulfill the highest-roughness row: identity initial.
    promises[W - 1].set_value({1.0, 1.0, 0.0});

    // Thread count capped at width (no need for more threads than rows).
    const uint32_t cap = m_threadCount < W ? m_threadCount : W;
    std::counting_semaphore<> sem(static_cast<std::ptrdiff_t>(cap));

    std::vector<std::thread> threads;
    threads.reserve(W);

    for (int a = static_cast<int>(W) - 1; a >= 0; --a) {
        sem.acquire();
        uint32_t ua = static_cast<uint32_t>(a);
        threads.emplace_back([ua, H, &worker, &output, &promises, &futures, &sem]() {
            // Wait for this row's t=0 initial guess from the predecessor row.
            LTCInitialGuess rowInit = futures[ua].get();

            // Fit t=0 cell.
            SchedulerJob j0{ua, 0, rowInit};
            output.at(ua, 0) = worker(j0);

            // Signal the next-lower row that its t=0 initial is ready.
            if (ua > 0) {
                promises[ua - 1].set_value(
                    normalIncidenceInitFromCell(output.at(ua, 0)));
            }

            // Fit remaining cells in this row sequentially.
            for (uint32_t t = 1; t < H; ++t) {
                LTCInitialGuess init = initialFromCell(output.at(ua, t - 1));
                SchedulerJob jt{ua, t, init};
                output.at(ua, t) = worker(jt);
            }

            sem.release();
        });
    }

    for (auto& thr : threads) thr.join();
}

} // namespace ltc
