#pragma once
#include <cstdint>
#include <vector>
#include <cstddef>

namespace ltc {

enum class TextureFormat { RG16F, RGBA16F, RG32F, RGBA32F };

uint32_t channelCount(TextureFormat fmt);
uint32_t bytesPerPixel(TextureFormat fmt);

struct TextureData {
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat format = TextureFormat::RGBA16F;
    std::vector<std::byte> pixels; // row-major: pixelIndex = y*width + x
};

} // namespace ltc
