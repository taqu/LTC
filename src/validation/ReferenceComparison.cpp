#include "ltc/ReferenceComparison.h"
#include "ltc/DDSReader.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ltc {

namespace {

struct ChannelAccumulator {
    double sumAE = 0;
    double sumSE = 0;
    double sumRE = 0;
    double maxAE = 0;
    double maxRE = 0;
    double minVal = 0, maxVal = 0;
    double minRef = 0, maxRef = 0;
    uint32_t count = 0;
    uint32_t nanCount = 0;
    uint32_t infCount = 0;
    uint32_t exceedCount = 0;
    uint32_t worstX = 0, worstY = 0;
    bool initialized = false;
};

void accumulatePixel(
    ChannelAccumulator& acc,
    double val, double ref,
    uint32_t x, uint32_t y,
    double relativeErrorFloor,
    double threshold)
{
    if (std::isnan(val) || std::isnan(ref)) {
        ++acc.nanCount;
        return;
    }
    if (std::isinf(val) || std::isinf(ref)) {
        ++acc.infCount;
        return;
    }

    if (!acc.initialized) {
        acc.minVal = val;
        acc.maxVal = val;
        acc.minRef = ref;
        acc.maxRef = ref;
        acc.initialized = true;
    } else {
        acc.minVal = std::min(acc.minVal, val);
        acc.maxVal = std::max(acc.maxVal, val);
        acc.minRef = std::min(acc.minRef, ref);
        acc.maxRef = std::max(acc.maxRef, ref);
    }

    double ae = std::abs(val - ref);
    acc.sumAE += ae;
    acc.sumSE += ae * ae;

    double refAbs = std::max(std::abs(ref), relativeErrorFloor);
    double re = ae / refAbs;
    acc.sumRE += re;

    if (ae > acc.maxAE) {
        acc.maxAE = ae;
        acc.worstX = x;
        acc.worstY = y;
    }
    if (re > acc.maxRE) {
        acc.maxRE = re;
    }
    if (ae > threshold) {
        ++acc.exceedCount;
    }
    ++acc.count;
}

ChannelComparisonStats finalize(const ChannelAccumulator& acc)
{
    ChannelComparisonStats s;
    s.minVal = acc.minVal;
    s.maxVal = acc.maxVal;
    s.minRef = acc.minRef;
    s.maxRef = acc.maxRef;
    s.maxAE = acc.maxAE;
    s.maxRE = acc.maxRE;
    s.nanCount = acc.nanCount;
    s.infCount = acc.infCount;
    s.exceedCount = acc.exceedCount;
    s.worstX = acc.worstX;
    s.worstY = acc.worstY;

    if (acc.count > 0) {
        double n = static_cast<double>(acc.count);
        s.mae = acc.sumAE / n;
        s.rmse = std::sqrt(acc.sumSE / n);
        s.mre = acc.sumRE / n;
    }
    return s;
}

} // anonymous namespace

TextureComparisonResult compareTextures(
    const std::filesystem::path& generated,
    const std::filesystem::path& reference,
    double relativeErrorFloor,
    double threshold)
{
    TextureComparisonResult result;

    // Read metadata
    DDSMetadata genMeta, refMeta;
    try {
        genMeta = DDSReader::readMetadata(generated);
    } catch (const std::exception& e) {
        result.compatible = false;
        result.incompatibilityReason = std::string("cannot read generated file: ") + e.what();
        return result;
    }

    try {
        refMeta = DDSReader::readMetadata(reference);
    } catch (const std::exception& e) {
        result.compatible = false;
        result.incompatibilityReason = std::string("cannot read reference file: ") + e.what();
        return result;
    }

    // Check compatibility
    if (genMeta.width != refMeta.width || genMeta.height != refMeta.height) {
        result.compatible = false;
        result.incompatibilityReason = "dimension mismatch: generated="
            + std::to_string(genMeta.width) + "x" + std::to_string(genMeta.height)
            + " reference=" + std::to_string(refMeta.width) + "x" + std::to_string(refMeta.height);
        return result;
    }

    if (genMeta.channelCount != refMeta.channelCount) {
        result.compatible = false;
        result.incompatibilityReason = "channel count mismatch: generated="
            + std::to_string(genMeta.channelCount)
            + " reference=" + std::to_string(refMeta.channelCount);
        return result;
    }

    result.compatible = true;
    result.width = genMeta.width;
    result.height = genMeta.height;
    result.channelCount = genMeta.channelCount;

    uint32_t W = genMeta.width;
    uint32_t H = genMeta.height;
    uint32_t C = genMeta.channelCount;

    // Allocate accumulators for global and 9 regions (3x3 grid)
    std::vector<ChannelAccumulator> globalAcc(C);

    // Region boundaries (3x3: low/mid/high roughness × normal/mid/grazing view)
    uint32_t rx[4] = { 0, W / 3, 2 * W / 3, W };
    uint32_t ry[4] = { 0, H / 3, 2 * H / 3, H };
    std::string regionNames[3][3] = {
        {"low-roughness-normal-view", "low-roughness-mid-view", "low-roughness-grazing-view"},
        {"mid-roughness-normal-view", "mid-roughness-mid-view", "mid-roughness-grazing-view"},
        {"high-roughness-normal-view", "high-roughness-mid-view", "high-roughness-grazing-view"},
    };

    // Create region accumulators: [roughnessRegion][viewRegion][channel]
    std::vector<std::vector<std::vector<ChannelAccumulator>>> regionAcc(3,
        std::vector<std::vector<ChannelAccumulator>>(3,
            std::vector<ChannelAccumulator>(C)));

    // Iterate over all pixels
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            Pixel genPix, refPix;
            try {
                genPix = DDSReader::readPixel(generated, x, y);
                refPix = DDSReader::readPixel(reference, x, y);
            } catch (...) {
                continue;
            }

            // Determine which region this pixel belongs to
            int roughRegion = -1, viewRegion = -1;
            for (int r = 0; r < 3; ++r) {
                if (x >= rx[r] && x < rx[r + 1]) { roughRegion = r; break; }
            }
            for (int v = 0; v < 3; ++v) {
                if (y >= ry[v] && y < ry[v + 1]) { viewRegion = v; break; }
            }

            for (uint32_t c = 0; c < C && c < 4; ++c) {
                double val = static_cast<double>(genPix.channels[c]);
                double ref = static_cast<double>(refPix.channels[c]);
                accumulatePixel(globalAcc[c], val, ref, x, y, relativeErrorFloor, threshold);
                if (roughRegion >= 0 && viewRegion >= 0) {
                    accumulatePixel(regionAcc[roughRegion][viewRegion][c],
                        val, ref, x, y, relativeErrorFloor, threshold);
                }
            }
        }
    }

    // Finalize global stats
    result.channels.resize(C);
    for (uint32_t c = 0; c < C; ++c) {
        result.channels[c] = finalize(globalAcc[c]);
    }

    // Finalize region stats
    for (int rr = 0; rr < 3; ++rr) {
        for (int vr = 0; vr < 3; ++vr) {
            RegionStats rs;
            rs.name = regionNames[rr][vr];
            rs.x0 = rx[rr]; rs.x1 = rx[rr + 1];
            rs.y0 = ry[vr]; rs.y1 = ry[vr + 1];
            rs.channels.resize(C);
            for (uint32_t c = 0; c < C; ++c) {
                rs.channels[c] = finalize(regionAcc[rr][vr][c]);
            }
            result.regions.push_back(std::move(rs));
        }
    }

    return result;
}

} // namespace ltc
