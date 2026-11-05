#pragma once
#include "DDS.h"
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace ltc {

class DDSError : public std::runtime_error {
public:
    explicit DDSError(const std::string& msg) : std::runtime_error(msg) {}
};

struct ChannelStats {
    double min  = 0.0;
    double max  = 0.0;
    double mean = 0.0;
    uint64_t nanCount  = 0;
    uint64_t posInfCount = 0;
    uint64_t negInfCount = 0;
    bool hasFinite = false;
};

struct ImageStats {
    std::array<ChannelStats, 4> channels{};
    uint32_t channelCount = 0;
};

class DDSReader {
public:
    // Throws DDSError on failure.
    static DDSMetadata readMetadata(const std::filesystem::path& path);

    // Reads a single pixel from the first mip level of a non-array 2D texture.
    // (0,0) is the top-left pixel.
    // Throws DDSError on failure or out-of-range coordinates.
    static Pixel readPixel(const std::filesystem::path& path, uint32_t x, uint32_t y);

    // Computes per-channel statistics over the first mip level.
    // Throws DDSError on failure.
    static ImageStats computeStats(const std::filesystem::path& path);

    // Converts an IEEE 754 binary16 bit pattern to float32.
    static float fp16ToFloat(uint16_t h) noexcept;
};

} // namespace ltc
