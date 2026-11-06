#include <gtest/gtest.h>
#include "ltc/ReferenceComparison.h"
#include "ltc/DDSWriter.h"
#include "ltc/DDSReader.h"
#include "ltc/Float16.h"
#include "ltc/TextureData.h"
#include <filesystem>
#include <cstring>

using namespace ltc;

namespace {

// Helper: create a TextureData with all channels set to a constant value
static TextureData makeConstantTexture(uint32_t w, uint32_t h,
                                        TextureFormat fmt, float value)
{
    TextureData tex;
    tex.width  = w;
    tex.height = h;
    tex.format = fmt;
    uint32_t bpp = bytesPerPixel(fmt);
    tex.pixels.resize(static_cast<size_t>(w) * h * bpp, std::byte{0});

    uint32_t nCh = channelCount(fmt);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            std::byte* dst = tex.pixels.data() + (static_cast<size_t>(y) * w + x) * bpp;
            for (uint32_t c = 0; c < nCh; ++c) {
                if (fmt == TextureFormat::RG16F || fmt == TextureFormat::RGBA16F) {
                    uint16_t h16 = float32ToFloat16(value);
                    std::memcpy(dst + c * 2, &h16, 2);
                } else {
                    std::memcpy(dst + c * 4, &value, 4);
                }
            }
        }
    }
    return tex;
}

static std::filesystem::path tempPath(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("ltc_refcomp_" + name + ".dds");
}

} // anonymous namespace

class ReferenceComparisonTest : public ::testing::Test {
protected:
    void TearDown() override {
        for (const auto& p : createdFiles_) {
            std::error_code ec;
            std::filesystem::remove(p, ec);
        }
    }

    std::filesystem::path createTemp(const std::string& name) {
        auto p = tempPath(name);
        createdFiles_.push_back(p);
        return p;
    }

    std::vector<std::filesystem::path> createdFiles_;
};

// --- Test 1: Identical textures have zero error ---
TEST_F(ReferenceComparisonTest, IdenticalTexturesZeroError)
{
    auto genPath = createTemp("gen_identical");
    auto refPath = createTemp("ref_identical");

    TextureData tex = makeConstantTexture(4, 4, TextureFormat::RGBA16F, 0.5f);
    DDSWriter::write(genPath, tex);
    DDSWriter::write(refPath, tex);

    TextureComparisonResult result = compareTextures(genPath, refPath, 1e-4, 0.01);

    ASSERT_TRUE(result.compatible);
    EXPECT_EQ(result.width, 4u);
    EXPECT_EQ(result.height, 4u);
    EXPECT_EQ(result.channelCount, 4u);

    for (const auto& ch : result.channels) {
        EXPECT_NEAR(ch.mae, 0.0, 1e-4);  // small due to fp16 round-trip
        EXPECT_NEAR(ch.rmse, 0.0, 1e-4);
    }
}

// --- Test 2: Dimension mismatch gives incompatible ---
TEST_F(ReferenceComparisonTest, DimensionMismatchIncompatible)
{
    auto genPath = createTemp("gen_dim");
    auto refPath = createTemp("ref_dim");

    TextureData genTex = makeConstantTexture(4, 4, TextureFormat::RGBA16F, 0.5f);
    TextureData refTex = makeConstantTexture(8, 8, TextureFormat::RGBA16F, 0.5f);
    DDSWriter::write(genPath, genTex);
    DDSWriter::write(refPath, refTex);

    TextureComparisonResult result = compareTextures(genPath, refPath, 1e-4, 0.01);

    EXPECT_FALSE(result.compatible);
    EXPECT_FALSE(result.incompatibilityReason.empty());
}

// --- Test 3: Missing file gives incompatible ---
TEST_F(ReferenceComparisonTest, MissingFileIncompatible)
{
    auto genPath = createTemp("gen_missing");
    // Don't create ref file
    std::filesystem::path refPath = tempPath("ref_missing_nonexistent_12345");

    TextureData genTex = makeConstantTexture(4, 4, TextureFormat::RGBA16F, 0.5f);
    DDSWriter::write(genPath, genTex);

    TextureComparisonResult result = compareTextures(genPath, refPath, 1e-4, 0.01);

    EXPECT_FALSE(result.compatible);
    EXPECT_FALSE(result.incompatibilityReason.empty());
}

// --- Test 4: Known offset produces non-zero MAE ---
TEST_F(ReferenceComparisonTest, KnownOffsetProducesError)
{
    auto genPath = createTemp("gen_offset");
    auto refPath = createTemp("ref_offset");

    // Use fp32 for exact arithmetic
    TextureData genTex = makeConstantTexture(4, 4, TextureFormat::RGBA32F, 0.5f);
    TextureData refTex = makeConstantTexture(4, 4, TextureFormat::RGBA32F, 0.6f);
    DDSWriter::write(genPath, genTex);
    DDSWriter::write(refPath, refTex);

    TextureComparisonResult result = compareTextures(genPath, refPath, 1e-4, 0.01);

    ASSERT_TRUE(result.compatible);
    ASSERT_FALSE(result.channels.empty());

    // Expect MAE ≈ 0.1 for all channels
    for (const auto& ch : result.channels) {
        EXPECT_NEAR(ch.mae, 0.1, 1e-4);
        EXPECT_NEAR(ch.maxAE, 0.1, 1e-4);
    }
}

// --- Test 5: Channel count mismatch gives incompatible ---
TEST_F(ReferenceComparisonTest, ChannelCountMismatchIncompatible)
{
    auto genPath = createTemp("gen_chan");
    auto refPath = createTemp("ref_chan");

    TextureData genTex = makeConstantTexture(4, 4, TextureFormat::RG16F, 0.5f);    // 2 channels
    TextureData refTex = makeConstantTexture(4, 4, TextureFormat::RGBA16F, 0.5f);  // 4 channels
    DDSWriter::write(genPath, genTex);
    DDSWriter::write(refPath, refTex);

    TextureComparisonResult result = compareTextures(genPath, refPath, 1e-4, 0.01);

    EXPECT_FALSE(result.compatible);
}

// --- Test 6: Regions are populated ---
TEST_F(ReferenceComparisonTest, RegionsPopulated)
{
    auto genPath = createTemp("gen_regions");
    auto refPath = createTemp("ref_regions");

    TextureData tex = makeConstantTexture(9, 9, TextureFormat::RGBA32F, 0.5f);
    DDSWriter::write(genPath, tex);
    DDSWriter::write(refPath, tex);

    TextureComparisonResult result = compareTextures(genPath, refPath, 1e-4, 0.01);

    ASSERT_TRUE(result.compatible);
    EXPECT_EQ(result.regions.size(), 9u);  // 3x3 grid

    for (const auto& reg : result.regions) {
        EXPECT_FALSE(reg.name.empty());
        EXPECT_EQ(reg.channels.size(), 4u);
    }
}

// --- Test 7: Exceed count counts pixels above threshold ---
TEST_F(ReferenceComparisonTest, ExceedCountWorks)
{
    auto genPath = createTemp("gen_exceed");
    auto refPath = createTemp("ref_exceed");

    // All pixels differ by 0.2, threshold is 0.1 → all pixels exceed
    TextureData genTex = makeConstantTexture(4, 4, TextureFormat::RGBA32F, 0.5f);
    TextureData refTex = makeConstantTexture(4, 4, TextureFormat::RGBA32F, 0.7f);
    DDSWriter::write(genPath, genTex);
    DDSWriter::write(refPath, refTex);

    TextureComparisonResult result = compareTextures(genPath, refPath, 1e-4, 0.1);

    ASSERT_TRUE(result.compatible);
    for (const auto& ch : result.channels) {
        EXPECT_EQ(ch.exceedCount, 16u);  // all 4x4=16 pixels exceed
    }
}
