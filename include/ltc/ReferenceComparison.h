#pragma once
#include "DDS.h"
#include <filesystem>
#include <string>
#include <vector>

namespace ltc {

struct ChannelComparisonStats {
    double minVal = 0, maxVal = 0;
    double minRef = 0, maxRef = 0;
    double mae = 0;      // mean absolute error
    double maxAE = 0;    // max absolute error
    double rmse = 0;     // root mean squared error
    double mre = 0;      // mean relative error (denominator floor applied)
    double maxRE = 0;
    uint32_t exceedCount = 0;    // values exceeding threshold
    uint32_t nanCount = 0;
    uint32_t infCount = 0;
    uint32_t worstX = 0, worstY = 0;
};

struct RegionStats {
    std::string name;
    uint32_t x0, x1, y0, y1;    // pixel region [x0,x1) x [y0,y1)
    std::vector<ChannelComparisonStats> channels;
};

struct TextureComparisonResult {
    bool compatible = false;
    std::string incompatibilityReason;
    uint32_t width = 0, height = 0;
    uint32_t channelCount = 0;
    std::vector<ChannelComparisonStats> channels;  // per-channel global stats
    std::vector<RegionStats> regions;              // per-region stats
};

// Compare two DDS textures. Returns incompatibility reason if dimensions/formats differ.
TextureComparisonResult compareTextures(
    const std::filesystem::path& generated,
    const std::filesystem::path& reference,
    double relativeErrorFloor = 1e-4,
    double threshold = 0.01);

} // namespace ltc
