#include "ltc/DDSWriter.h"
#include "ltc/DDSReader.h"
#include "ltc/Float16.h"
#include "ltc/TextureData.h"
#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace {

// Helper: create a simple TextureData with known pixel values
static ltc::TextureData makeTestTexture(uint32_t w, uint32_t h, ltc::TextureFormat fmt) {
    ltc::TextureData tex;
    tex.width  = w;
    tex.height = h;
    tex.format = fmt;

    uint32_t bpp = ltc::bytesPerPixel(fmt);
    tex.pixels.resize(static_cast<size_t>(w) * h * bpp, std::byte{0});

    // Fill with simple values: pixel(x,y) = (x+1) * (y+1) * channel
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            std::byte* dst = tex.pixels.data() + (static_cast<size_t>(y) * w + x) * bpp;
            uint32_t nCh = ltc::channelCount(fmt);

            for (uint32_t c = 0; c < nCh; ++c) {
                float val = static_cast<float>((x + 1) * (y + 1)) * (c + 1) * 0.1f;
                if (fmt == ltc::TextureFormat::RG16F || fmt == ltc::TextureFormat::RGBA16F) {
                    uint16_t h16 = ltc::float32ToFloat16(val);
                    std::memcpy(dst + c * 2, &h16, 2);
                } else {
                    std::memcpy(dst + c * 4, &val, 4);
                }
            }
        }
    }
    return tex;
}

// Helper: create a temporary file path
static std::filesystem::path tempPath(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("ltc_test_" + name + ".dds");
}

class DDSWriterTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (const auto& p : createdFiles) {
            std::error_code ec;
            std::filesystem::remove(p, ec);
        }
    }

    std::filesystem::path createTemp(const std::string& name) {
        auto p = tempPath(name);
        createdFiles.push_back(p);
        return p;
    }

    std::vector<std::filesystem::path> createdFiles;
};

TEST_F(DDSWriterTest, WriteRG16F_MetadataRoundTrip) {
    auto path = createTemp("rg16f_meta");
    ltc::TextureData tex = makeTestTexture(4, 3, ltc::TextureFormat::RG16F);

    ltc::DDSWriter::write(path, tex);
    ASSERT_TRUE(std::filesystem::exists(path));

    ltc::DDSMetadata meta = ltc::DDSReader::readMetadata(path);
    EXPECT_EQ(meta.width,        4u);
    EXPECT_EQ(meta.height,       3u);
    EXPECT_EQ(meta.format,       ltc::DDSFormat::RG16F);
    EXPECT_EQ(meta.channelCount, 2u);
    EXPECT_EQ(meta.bytesPerPixel, 4u);
}

TEST_F(DDSWriterTest, WriteRGBA16F_PayloadSize) {
    auto path = createTemp("rgba16f_size");
    ltc::TextureData tex = makeTestTexture(8, 4, ltc::TextureFormat::RGBA16F);

    ltc::DDSWriter::write(path, tex);
    ASSERT_TRUE(std::filesystem::exists(path));

    ltc::DDSMetadata meta = ltc::DDSReader::readMetadata(path);
    EXPECT_EQ(meta.width,  8u);
    EXPECT_EQ(meta.height, 4u);
    EXPECT_EQ(meta.format, ltc::DDSFormat::RGBA16F);
    // RGBA16F: 8 bytes per pixel
    EXPECT_EQ(meta.dataSize, 8u * 4u * 8u); // width * height * bpp
}

TEST_F(DDSWriterTest, WriteRG32F_DXGIFormat) {
    auto path = createTemp("rg32f_dxgi");
    ltc::TextureData tex = makeTestTexture(2, 2, ltc::TextureFormat::RG32F);

    ltc::DDSWriter::write(path, tex);
    ASSERT_TRUE(std::filesystem::exists(path));

    ltc::DDSMetadata meta = ltc::DDSReader::readMetadata(path);
    EXPECT_EQ(meta.format,       ltc::DDSFormat::RG32F);
    EXPECT_EQ(meta.channelCount, 2u);
    EXPECT_EQ(meta.bytesPerPixel, 8u); // 2 * 4 bytes
    // Verify data size: 2*2*8 = 32
    EXPECT_EQ(meta.dataSize, 32u);
}

TEST_F(DDSWriterTest, WriteRGBA32F_DXGIFormat) {
    auto path = createTemp("rgba32f_dxgi");
    ltc::TextureData tex = makeTestTexture(2, 2, ltc::TextureFormat::RGBA32F);

    ltc::DDSWriter::write(path, tex);
    ASSERT_TRUE(std::filesystem::exists(path));

    ltc::DDSMetadata meta = ltc::DDSReader::readMetadata(path);
    EXPECT_EQ(meta.format,       ltc::DDSFormat::RGBA32F);
    EXPECT_EQ(meta.channelCount, 4u);
    EXPECT_EQ(meta.bytesPerPixel, 16u); // 4 * 4 bytes
}

TEST_F(DDSWriterTest, InvalidDimensionsThrows) {
    ltc::TextureData tex;
    tex.width  = 0;
    tex.height = 4;
    tex.format = ltc::TextureFormat::RGBA16F;
    tex.pixels.resize(0);

    auto path = createTemp("invalid_dim");
    EXPECT_THROW(ltc::DDSWriter::write(path, tex), std::runtime_error);
}

TEST_F(DDSWriterTest, MismatchedPixelSizeThrows) {
    ltc::TextureData tex;
    tex.width  = 4;
    tex.height = 4;
    tex.format = ltc::TextureFormat::RGBA16F;
    tex.pixels.resize(10, std::byte{0}); // wrong size

    auto path = createTemp("mismatch_size");
    EXPECT_THROW(ltc::DDSWriter::write(path, tex), std::runtime_error);
}

TEST_F(DDSWriterTest, Write1x1Texture) {
    auto path = createTemp("1x1");
    ltc::TextureData tex;
    tex.width  = 1;
    tex.height = 1;
    tex.format = ltc::TextureFormat::RGBA16F;
    tex.pixels.resize(8, std::byte{0});

    // Write 0.5, 1.0, 0.0, 2.0 as FP16
    float vals[] = {0.5f, 1.0f, 0.0f, 2.0f};
    for (int i = 0; i < 4; ++i) {
        uint16_t h = ltc::float32ToFloat16(vals[i]);
        std::memcpy(tex.pixels.data() + i * 2, &h, 2);
    }

    ltc::DDSWriter::write(path, tex);
    ltc::DDSMetadata meta = ltc::DDSReader::readMetadata(path);
    EXPECT_EQ(meta.width,  1u);
    EXPECT_EQ(meta.height, 1u);
    EXPECT_EQ(meta.format, ltc::DDSFormat::RGBA16F);
}

} // namespace
