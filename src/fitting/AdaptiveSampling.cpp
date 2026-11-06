#include "ltc/AdaptiveSampling.h"

namespace ltc {

AdaptiveSamplingSchedule::AdaptiveSamplingSchedule(
    uint32_t initialResolution, uint32_t maximumResolution)
{
    if (initialResolution == 0) initialResolution = 1;
    if (maximumResolution  < initialResolution) maximumResolution = initialResolution;

    uint32_t r = initialResolution;
    for (;;) {
        m_stages.push_back({r});
        if (r >= maximumResolution) break;
        uint32_t next = r * 2;
        if (next > maximumResolution) next = maximumResolution;
        r = next;
    }
}

bool AdaptiveSamplingSchedule::hasNextStage(uint32_t currentResolution) const
{
    for (size_t i = 0; i + 1 < m_stages.size(); ++i) {
        if (m_stages[i].resolution == currentResolution)
            return true;
    }
    return false;
}

uint32_t AdaptiveSamplingSchedule::nextResolution(uint32_t currentResolution) const
{
    for (size_t i = 0; i + 1 < m_stages.size(); ++i) {
        if (m_stages[i].resolution == currentResolution)
            return m_stages[i + 1].resolution;
    }
    return currentResolution; // already at maximum
}

} // namespace ltc
