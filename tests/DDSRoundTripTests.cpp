#include "ltc/DDSWriter.h"
#include "ltc/DDSReader.h"
#include "ltc/Float16.h"
#include "ltc/TextureData.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace {

static std::filesystem::path tempPath(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("ltc_roundtrip_" + name + ".dds");
}

class DDSRoundTripTest : public ::testing::Test {
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

// Known float values for round-trip testing
static const std::array<float, 8> kTestValues = {
    0.0f, 1.0f, -1.0f, 0.5f, 2.0f, 100.0f, -0.25f, 0.125f
};

TEST_F(DDSRoundTripTest, RoundTrip_RG16F) {
    uint32_t w = 4, h = 2;
    ltc::TextureData tex;
    tex.width  = w;
    tex.height = h;
    tex.format = ltc::TextureFormat::RG16F;

    uint32_t bpp = ltc::bytesPerPixel(ltc::TextureFormat::RG16F);
    tex.pixels.resize(static_cast<size_t>(w) * h * bpp, std::byte{0});

    // Fill with test values
    std::vector<std::array<float, 2>> origVals(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t idx = y * w + x;
            origVals[idx][0] = kTestValues[idx % kTestValues.size()];
            origVals[idx][1] = kTestValues[(idx + 1) % kTestValues.size()];

            std::byte* dst = tex.pixels.data() + idx * bpp;
            uint16_t r16 = ltc::float32ToFloat16(origVals[idx][0]);
            uint16_t g16 = ltc::float32ToFloat16(origVals[idx][1]);
            std::memcpy(dst + 0, &r16, 2);
            std::memcpy(dst + 2, &g16, 2);
        }
    }

    auto path = createTemp("rg16f");
    ltc::DDSWriter::write(path, tex);

    // Read back and compare
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            ltc::Pixel pix = ltc::DDSReader::readPixel(path, x, y);
            EXPECT_EQ(pix.channelCount, 2u);

            uint32_t idx = y * w + x;
            // FP16 round-trip: encode original with float32ToFloat16, decode with fp16ToFloat
            float expected0 = ltc::DDSReader::fp16ToFloat(ltc::float32ToFloat16(origVals[idx][0]));
            float expected1 = ltc::DDSReader::fp16ToFloat(ltc::float32ToFloat16(origVals[idx][1]));

            EXPECT_FLOAT_EQ(pix.channels[0], expected0) << "x=" << x << " y=" << y << " ch0";
            EXPECT_FLOAT_EQ(pix.channels[1], expected1) << "x=" << x << " y=" << y << " ch1";
        }
    }
}

TEST_F(DDSRoundTripTest, RoundTrip_RGBA16F) {
    uint32_t w = 2, h = 3;
    ltc::TextureData tex;
    tex.width  = w;
    tex.height = h;
    tex.format = ltc::TextureFormat::RGBA16F;

    uint32_t bpp = ltc::bytesPerPixel(ltc::TextureFormat::RGBA16F);
    tex.pixels.resize(static_cast<size_t>(w) * h * bpp, std::byte{0});

    std::vector<std::array<float, 4>> origVals(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t idx = y * w + x;
            for (int c = 0; c < 4; ++c) {
                origVals[idx][c] = kTestValues[(idx * 4 + c) % kTestValues.size()];
            }

            std::byte* dst = tex.pixels.data() + idx * bpp;
            for (int c = 0; c < 4; ++c) {
                uint16_t h16 = ltc::float32ToFloat16(origVals[idx][c]);
                std::memcpy(dst + c * 2, &h16, 2);
            }
        }
    }

    auto path = createTemp("rgba16f");
    ltc::DDSWriter::write(path, tex);

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            ltc::Pixel pix = ltc::DDSReader::readPixel(path, x, y);
            EXPECT_EQ(pix.channelCount, 4u);

            uint32_t idx = y * w + x;
            for (int c = 0; c < 4; ++c) {
                float expected = ltc::DDSReader::fp16ToFloat(ltc::float32ToFloat16(origVals[idx][c]));
                EXPECT_FLOAT_EQ(pix.channels[c], expected)
                    << "x=" << x << " y=" << y << " ch=" << c;
            }
        }
    }
}

TEST_F(DDSRoundTripTest, RoundTrip_RG32F) {
    uint32_t w = 3, h = 2;
    ltc::TextureData tex;
    tex.width  = w;
    tex.height = h;
    tex.format = ltc::TextureFormat::RG32F;

    uint32_t bpp = ltc::bytesPerPixel(ltc::TextureFormat::RG32F);
    tex.pixels.resize(static_cast<size_t>(w) * h * bpp, std::byte{0});

    std::vector<std::array<float, 2>> origVals(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t idx = y * w + x;
            origVals[idx][0] = kTestValues[idx % kTestValues.size()];
            origVals[idx][1] = kTestValues[(idx + 2) % kTestValues.size()];

            std::byte* dst = tex.pixels.data() + idx * bpp;
            std::memcpy(dst + 0, &origVals[idx][0], 4);
            std::memcpy(dst + 4, &origVals[idx][1], 4);
        }
    }

    auto path = createTemp("rg32f");
    ltc::DDSWriter::write(path, tex);

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            ltc::Pixel pix = ltc::DDSReader::readPixel(path, x, y);
            EXPECT_EQ(pix.channelCount, 2u);

            uint32_t idx = y * w + x;
            // FP32: exact round-trip
            EXPECT_NEAR(pix.channels[0], origVals[idx][0], 1e-6f)
                << "x=" << x << " y=" << y << " ch0";
            EXPECT_NEAR(pix.channels[1], origVals[idx][1], 1e-6f)
                << "x=" << x << " y=" << y << " ch1";
        }
    }
}

TEST_F(DDSRoundTripTest, RoundTrip_RGBA32F) {
    uint32_t w = 2, h = 2;
    ltc::TextureData tex;
    tex.width  = w;
    tex.height = h;
    tex.format = ltc::TextureFormat::RGBA32F;

    uint32_t bpp = ltc::bytesPerPixel(ltc::TextureFormat::RGBA32F);
    tex.pixels.resize(static_cast<size_t>(w) * h * bpp, std::byte{0});

    std::vector<std::array<float, 4>> origVals(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t idx = y * w + x;
            for (int c = 0; c < 4; ++c) {
                origVals[idx][c] = kTestValues[(idx * 4 + c + 1) % kTestValues.size()];
            }

            std::byte* dst = tex.pixels.data() + idx * bpp;
            for (int c = 0; c < 4; ++c) {
                std::memcpy(dst + c * 4, &origVals[idx][c], 4);
            }
        }
    }

    auto path = createTemp("rgba32f");
    ltc::DDSWriter::write(path, tex);

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            ltc::Pixel pix = ltc::DDSReader::readPixel(path, x, y);
            EXPECT_EQ(pix.channelCount, 4u);

            uint32_t idx = y * w + x;
            for (int c = 0; c < 4; ++c) {
                EXPECT_NEAR(pix.channels[c], origVals[idx][c], 1e-6f)
                    << "x=" << x << " y=" << y << " ch=" << c;
            }
        }
    }
}

TEST_F(DDSRoundTripTest, FileSize_RG16F) {
    uint32_t w = 8, h = 8;
    ltc::TextureData tex;
    tex.width  = w;
    tex.height = h;
    tex.format = ltc::TextureFormat::RG16F;
    tex.pixels.resize(static_cast<size_t>(w) * h * 4, std::byte{0});

    auto path = createTemp("rg16f_size");
    ltc::DDSWriter::write(path, tex);

    // Expected size: 4 (magic) + 124 (header) + 20 (DX10 header) + w*h*4 (data)
    uint64_t expectedSize = 4 + 124 + 20 + static_cast<uint64_t>(w) * h * 4;
    EXPECT_EQ(std::filesystem::file_size(path), expectedSize);
}

} // namespace
