#pragma once
#include <cstdint>
#include <vector>

namespace ltc {

struct SamplingStage
{
    uint32_t resolution;
};

// Builds a deterministic sequence of sampling resolutions from initialResolution
// to maximumResolution, doubling at each step.
//
// Example: initial=32, maximum=128 → [32, 64, 128]
// Example: initial=64, maximum=64  → [64]
class AdaptiveSamplingSchedule
{
public:
    AdaptiveSamplingSchedule(uint32_t initialResolution, uint32_t maximumResolution);

    const std::vector<SamplingStage>& stages()     const noexcept { return m_stages; }
    bool     hasNextStage(uint32_t currentResolution) const;
    uint32_t nextResolution(uint32_t currentResolution) const;

private:
    std::vector<SamplingStage> m_stages;
};

} // namespace ltc
