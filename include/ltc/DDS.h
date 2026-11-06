#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace ltc {

enum class DDSFormat : uint32_t {
    Unknown = 0,
    RG16F,
    RGBA16F,
    RG32F,
    RGBA32F,
};

struct DDSMetadata {
    uint32_t width       = 0;
    uint32_t height      = 0;
    uint32_t depth       = 1;
    uint32_t mipLevels   = 1;
    uint32_t arraySize   = 1;
    DDSFormat format     = DDSFormat::Unknown;
    uint32_t channelCount   = 0;
    uint32_t bytesPerPixel  = 0;
    uint64_t dataOffset  = 0;
    uint64_t dataSize    = 0;
};

struct Pixel {
    std::array<float, 4> channels{};
    uint32_t channelCount = 0;
};

std::string formatToString(DDSFormat fmt);
uint32_t formatChannelCount(DDSFormat fmt);
uint32_t formatBytesPerPixel(DDSFormat fmt);

} // namespace ltc
