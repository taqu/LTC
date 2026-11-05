#include "ltc/DDSReader.h"
#include "ltc/DDS.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// ============================================================================
// Synthetic DDS builder helpers
// ============================================================================

static constexpr uint32_t DDS_MAGIC                             = 0x20534444u;
static constexpr uint32_t DDPF_FOURCC                           = 0x4u;
static constexpr uint32_t FOURCC_DX10                           = 0x30315844u;
static constexpr uint32_t D3D10_RESOURCE_DIMENSION_TEXTURE2D    = 3u;
static constexpr uint32_t DXGI_FORMAT_R16G16B16A16_FLOAT        = 10u;
static constexpr uint32_t DXGI_FORMAT_R32G32B32A32_FLOAT        = 2u;
static constexpr uint32_t DXGI_FORMAT_R32G32_FLOAT              = 16u;
static constexpr uint32_t DXGI_FORMAT_R16G16_FLOAT              = 34u;
static constexpr uint32_t D3DFMT_A16B16G16R16F                  = 113u;
static constexpr uint32_t D3DFMT_G16R16F                        = 112u;

#pragma pack(push, 1)
struct TestDDSPixelFormat {
    uint32_t dwSize        = 32u;
    uint32_t dwFlags       = 0u;
    uint32_t dwFourCC      = 0u;
    uint32_t dwRGBBitCount = 0u;
    uint32_t dwRBitMask    = 0u;
    uint32_t dwGBitMask    = 0u;
    uint32_t dwBBitMask    = 0u;
    uint32_t dwABitMask    = 0u;
};
static_assert(sizeof(TestDDSPixelFormat) == 32);

struct TestDDSHeader {
    uint32_t dwSize              = 124u;
    uint32_t dwFlags             = 0x1007u; // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT
    uint32_t dwHeight            = 0u;
    uint32_t dwWidth             = 0u;
    uint32_t dwPitchOrLinearSize = 0u;
    uint32_t dwDepth             = 0u;
    uint32_t dwMipMapCount       = 1u;
    uint32_t dwReserved1[11]     = {};
    TestDDSPixelFormat ddspf;
    uint32_t dwCaps              = 0x1000u; // DDSCAPS_TEXTURE
    uint32_t dwCaps2             = 0u;
    uint32_t dwCaps3             = 0u;
    uint32_t dwCaps4             = 0u;
    uint32_t dwReserved2         = 0u;
};
static_assert(sizeof(TestDDSHeader) == 124);

struct TestDDSHeaderDXT10 {
    uint32_t dxgiFormat       = 0u;
    uint32_t resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    uint32_t miscFlag         = 0u;
    uint32_t arraySize        = 1u;
    uint32_t miscFlags2       = 0u;
};
static_assert(sizeof(TestDDSHeaderDXT10) == 20);
#pragma pack(pop)

// Build a DX10 RGBA16F DDS buffer in memory
static std::vector<uint8_t> makeDDSDX10RGBA16F(
    uint32_t w, uint32_t h,
    const std::vector<std::array<uint16_t, 4>>& pixels)
{
    std::vector<uint8_t> buf;
    buf.reserve(148 + w * h * 8);

    auto writeU32 = [&](uint32_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 8u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 16u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 24u) & 0xFFu));
    };
    auto writeU16 = [&](uint16_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 8u) & 0xFFu));
    };
    auto writeBytes = [&](const void* src, size_t n) {
        const auto* p = static_cast<const uint8_t*>(src);
        buf.insert(buf.end(), p, p + n);
    };

    // Magic
    writeU32(DDS_MAGIC);

    // Header
    TestDDSHeader hdr{};
    hdr.dwWidth  = w;
    hdr.dwHeight = h;
    hdr.ddspf.dwFlags  = DDPF_FOURCC;
    hdr.ddspf.dwFourCC = FOURCC_DX10;
    writeBytes(&hdr, sizeof(hdr));

    // DX10 extension
    TestDDSHeaderDXT10 dx10{};
    dx10.dxgiFormat        = DXGI_FORMAT_R16G16B16A16_FLOAT;
    dx10.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    dx10.arraySize         = 1u;
    writeBytes(&dx10, sizeof(dx10));

    // Pixel data
    for (auto& px : pixels) {
        writeU16(px[0]);
        writeU16(px[1]);
        writeU16(px[2]);
        writeU16(px[3]);
    }

    return buf;
}

// Build a legacy RGBA16F DDS buffer (FourCC=113)
static std::vector<uint8_t> makeDDSLegacyRGBA16F(
    uint32_t w, uint32_t h,
    const std::vector<std::array<uint16_t, 4>>& pixels)
{
    std::vector<uint8_t> buf;
    buf.reserve(128 + w * h * 8);

    auto writeU32 = [&](uint32_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 8u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 16u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 24u) & 0xFFu));
    };
    auto writeU16 = [&](uint16_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 8u) & 0xFFu));
    };
    auto writeBytes = [&](const void* src, size_t n) {
        const auto* p = static_cast<const uint8_t*>(src);
        buf.insert(buf.end(), p, p + n);
    };

    // Magic
    writeU32(DDS_MAGIC);

    // Header
    TestDDSHeader hdr{};
    hdr.dwWidth  = w;
    hdr.dwHeight = h;
    hdr.ddspf.dwFlags  = DDPF_FOURCC;
    hdr.ddspf.dwFourCC = D3DFMT_A16B16G16R16F; // 113
    writeBytes(&hdr, sizeof(hdr));

    // Pixel data (no DX10 header)
    for (auto& px : pixels) {
        writeU16(px[0]);
        writeU16(px[1]);
        writeU16(px[2]);
        writeU16(px[3]);
    }

    return buf;
}

// Write buffer to a temp file and return its path
static std::filesystem::path writeTempFile(const std::vector<uint8_t>& data,
                                           const std::string& suffix = ".dds") {
    auto tmp = std::filesystem::temp_directory_path() /
               ("ltc_test_" + std::to_string(
                   std::hash<std::string>{}(suffix + std::to_string(data.size()))) + suffix);
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
    return tmp;
}

// fp16 helper: encode a known fp32 value to fp16 bit pattern (simple cases only)
// 1.0f  = 0x3C00
// -1.0f = 0xBC00
// 0.5f  = 0x3800
// 2.0f  = 0x4000

// ============================================================================
// fp16ToFloat tests
// ============================================================================

TEST(FP16Test, Zero) {
    EXPECT_EQ(ltc::DDSReader::fp16ToFloat(0x0000u), 0.0f);
}

TEST(FP16Test, NegativeZero) {
    float v = ltc::DDSReader::fp16ToFloat(0x8000u);
    EXPECT_EQ(v, 0.0f);
    // Check sign bit
    uint32_t bits = 0;
    std::memcpy(&bits, &v, 4);
    EXPECT_EQ(bits, 0x80000000u);
}

TEST(FP16Test, One) {
    EXPECT_EQ(ltc::DDSReader::fp16ToFloat(0x3C00u), 1.0f);
}

TEST(FP16Test, NegativeOne) {
    EXPECT_EQ(ltc::DDSReader::fp16ToFloat(0xBC00u), -1.0f);
}

TEST(FP16Test, MaxFinite) {
    // 0x7BFF = 65504.0f
    EXPECT_FLOAT_EQ(ltc::DDSReader::fp16ToFloat(0x7BFFu), 65504.0f);
}

TEST(FP16Test, MinNormal) {
    // 0x0400 = 2^-14 ≈ 6.1035e-5
    float v = ltc::DDSReader::fp16ToFloat(0x0400u);
    EXPECT_FLOAT_EQ(v, std::ldexp(1.0f, -14));
}

TEST(FP16Test, MinSubnormal) {
    // 0x0001 = 2^-24 ≈ 5.96046e-8
    float v = ltc::DDSReader::fp16ToFloat(0x0001u);
    EXPECT_FLOAT_EQ(v, std::ldexp(1.0f, -24));
}

TEST(FP16Test, PositiveInfinity) {
    float v = ltc::DDSReader::fp16ToFloat(0x7C00u);
    EXPECT_TRUE(std::isinf(v));
    EXPECT_GT(v, 0.0f);
}

TEST(FP16Test, NegativeInfinity) {
    float v = ltc::DDSReader::fp16ToFloat(0xFC00u);
    EXPECT_TRUE(std::isinf(v));
    EXPECT_LT(v, 0.0f);
}

TEST(FP16Test, NaN) {
    float v = ltc::DDSReader::fp16ToFloat(0x7E00u);
    EXPECT_TRUE(std::isnan(v));
}

TEST(FP16Test, Half) {
    EXPECT_FLOAT_EQ(ltc::DDSReader::fp16ToFloat(0x3800u), 0.5f);
}

TEST(FP16Test, Two) {
    EXPECT_FLOAT_EQ(ltc::DDSReader::fp16ToFloat(0x4000u), 2.0f);
}

// ============================================================================
// Metadata – valid files
// ============================================================================

TEST(DDSReaderTest, ValidDX10RGBA16F) {
    uint32_t w = 4, h = 4;
    std::vector<std::array<uint16_t, 4>> pixels(w * h, {0x3C00u, 0x3800u, 0x0000u, 0x3C00u});
    auto buf  = makeDDSDX10RGBA16F(w, h, pixels);
    auto path = writeTempFile(buf);

    ltc::DDSMetadata meta = ltc::DDSReader::readMetadata(path);
    EXPECT_EQ(meta.width,         w);
    EXPECT_EQ(meta.height,        h);
    EXPECT_EQ(meta.format,        ltc::DDSFormat::RGBA16F);
    EXPECT_EQ(meta.channelCount,  4u);
    EXPECT_EQ(meta.bytesPerPixel, 8u);
    EXPECT_EQ(meta.dataOffset,    148u);
    EXPECT_EQ(meta.dataSize,      static_cast<uint64_t>(w) * h * 8u);
    EXPECT_EQ(meta.arraySize,     1u);
    EXPECT_EQ(meta.mipLevels,     1u);

    std::filesystem::remove(path);
}

TEST(DDSReaderTest, ValidLegacyRGBA16F) {
    uint32_t w = 2, h = 2;
    std::vector<std::array<uint16_t, 4>> pixels(w * h, {0x3C00u, 0x0000u, 0x0000u, 0x3C00u});
    auto buf  = makeDDSLegacyRGBA16F(w, h, pixels);
    auto path = writeTempFile(buf, "_legacy.dds");

    ltc::DDSMetadata meta = ltc::DDSReader::readMetadata(path);
    EXPECT_EQ(meta.width,         w);
    EXPECT_EQ(meta.height,        h);
    EXPECT_EQ(meta.format,        ltc::DDSFormat::RGBA16F);
    EXPECT_EQ(meta.channelCount,  4u);
    EXPECT_EQ(meta.bytesPerPixel, 8u);
    EXPECT_EQ(meta.dataOffset,    128u);
    EXPECT_EQ(meta.dataSize,      static_cast<uint64_t>(w) * h * 8u);

    std::filesystem::remove(path);
}

// ============================================================================
// Error cases
// ============================================================================

TEST(DDSReaderTest, InvalidMagic) {
    std::vector<uint8_t> buf(200, 0);
    buf[0] = 0xDE; buf[1] = 0xAD; buf[2] = 0xBE; buf[3] = 0xEF;
    // Fill header size fields to pass other checks (won't get there)
    auto path = writeTempFile(buf, "_badmagic.dds");
    EXPECT_THROW(ltc::DDSReader::readMetadata(path), ltc::DDSError);
    std::filesystem::remove(path);
}

TEST(DDSReaderTest, TruncatedHeader) {
    // Only 10 bytes — not enough for magic + header
    std::vector<uint8_t> buf = {0x44, 0x44, 0x53, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    auto path = writeTempFile(buf, "_trunc.dds");
    EXPECT_THROW(ltc::DDSReader::readMetadata(path), ltc::DDSError);
    std::filesystem::remove(path);
}

TEST(DDSReaderTest, WrongHeaderSize) {
    uint32_t w = 4, h = 4;
    std::vector<std::array<uint16_t, 4>> pixels(w * h, {});
    auto buf  = makeDDSDX10RGBA16F(w, h, pixels);
    // Corrupt dwSize field (bytes 4-7 of file, i.e. after magic)
    buf[4] = 99; buf[5] = 0; buf[6] = 0; buf[7] = 0;
    auto path = writeTempFile(buf, "_wrongsize.dds");
    EXPECT_THROW(ltc::DDSReader::readMetadata(path), ltc::DDSError);
    std::filesystem::remove(path);
}

TEST(DDSReaderTest, MissingDX10Extension) {
    // Build a DDS that claims DX10 FourCC but is too short for the DX10 header
    std::vector<uint8_t> buf;
    auto writeU32 = [&](uint32_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 8u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 16u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 24u) & 0xFFu));
    };
    writeU32(DDS_MAGIC);
    TestDDSHeader hdr{};
    hdr.dwWidth  = 4;
    hdr.dwHeight = 4;
    hdr.ddspf.dwFlags  = DDPF_FOURCC;
    hdr.ddspf.dwFourCC = FOURCC_DX10;
    const auto* hdrBytes = reinterpret_cast<const uint8_t*>(&hdr);
    buf.insert(buf.end(), hdrBytes, hdrBytes + sizeof(hdr));
    // No DX10 extension, no pixel data — file is exactly 128 bytes, too short for DX10 header

    auto path = writeTempFile(buf, "_noDX10ext.dds");
    EXPECT_THROW(ltc::DDSReader::readMetadata(path), ltc::DDSError);
    std::filesystem::remove(path);
}

TEST(DDSReaderTest, InvalidDimensions) {
    // Build a DX10 DDS with 0 width
    std::vector<uint8_t> buf;
    auto writeU32 = [&](uint32_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 8u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 16u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 24u) & 0xFFu));
    };
    writeU32(DDS_MAGIC);
    TestDDSHeader hdr{};
    hdr.dwWidth  = 0;  // invalid
    hdr.dwHeight = 4;
    hdr.ddspf.dwFlags  = DDPF_FOURCC;
    hdr.ddspf.dwFourCC = FOURCC_DX10;
    const auto* hdrBytes = reinterpret_cast<const uint8_t*>(&hdr);
    buf.insert(buf.end(), hdrBytes, hdrBytes + sizeof(hdr));
    // Add DX10 header to avoid short-file rejection first
    TestDDSHeaderDXT10 dx10{};
    dx10.dxgiFormat        = DXGI_FORMAT_R16G16B16A16_FLOAT;
    dx10.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    dx10.arraySize         = 1u;
    const auto* dx10Bytes = reinterpret_cast<const uint8_t*>(&dx10);
    buf.insert(buf.end(), dx10Bytes, dx10Bytes + sizeof(dx10));
    // Some padding pixel data
    buf.resize(buf.size() + 128, 0);

    auto path = writeTempFile(buf, "_zerodim.dds");
    EXPECT_THROW(ltc::DDSReader::readMetadata(path), ltc::DDSError);
    std::filesystem::remove(path);
}

TEST(DDSReaderTest, UnsupportedResourceDimension3D) {
    std::vector<uint8_t> buf;
    auto writeU32 = [&](uint32_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 8u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 16u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 24u) & 0xFFu));
    };
    writeU32(DDS_MAGIC);
    TestDDSHeader hdr{};
    hdr.dwWidth  = 4;
    hdr.dwHeight = 4;
    hdr.ddspf.dwFlags  = DDPF_FOURCC;
    hdr.ddspf.dwFourCC = FOURCC_DX10;
    const auto* hdrBytes = reinterpret_cast<const uint8_t*>(&hdr);
    buf.insert(buf.end(), hdrBytes, hdrBytes + sizeof(hdr));
    TestDDSHeaderDXT10 dx10{};
    dx10.dxgiFormat        = DXGI_FORMAT_R16G16B16A16_FLOAT;
    dx10.resourceDimension = 4u; // TEXTURE3D
    dx10.arraySize         = 1u;
    const auto* dx10Bytes = reinterpret_cast<const uint8_t*>(&dx10);
    buf.insert(buf.end(), dx10Bytes, dx10Bytes + sizeof(dx10));
    buf.resize(buf.size() + 128, 0);

    auto path = writeTempFile(buf, "_3d.dds");
    EXPECT_THROW(ltc::DDSReader::readMetadata(path), ltc::DDSError);
    std::filesystem::remove(path);
}

TEST(DDSReaderTest, TruncatedPixelPayload) {
    uint32_t w = 4, h = 4;
    std::vector<std::array<uint16_t, 4>> pixels(w * h, {0x3C00u, 0x0000u, 0x0000u, 0x3C00u});
    auto buf  = makeDDSDX10RGBA16F(w, h, pixels);
    // Truncate to remove the last half of pixel data
    buf.resize(buf.size() / 2);
    auto path = writeTempFile(buf, "_trunc_pixels.dds");
    EXPECT_THROW(ltc::DDSReader::readMetadata(path), ltc::DDSError);
    std::filesystem::remove(path);
}

TEST(DDSReaderTest, IntegerOverflowDimensions) {
    // Construct a DDS with huge width/height that would overflow uint64_t
    std::vector<uint8_t> buf;
    auto writeU32 = [&](uint32_t v) {
        buf.push_back(static_cast<uint8_t>(v & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 8u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 16u) & 0xFFu));
        buf.push_back(static_cast<uint8_t>((v >> 24u) & 0xFFu));
    };
    writeU32(DDS_MAGIC);
    TestDDSHeader hdr{};
    hdr.dwWidth  = 0xFFFFFFFFu; // max uint32
    hdr.dwHeight = 0xFFFFFFFFu;
    hdr.ddspf.dwFlags  = DDPF_FOURCC;
    hdr.ddspf.dwFourCC = FOURCC_DX10;
    const auto* hdrBytes = reinterpret_cast<const uint8_t*>(&hdr);
    buf.insert(buf.end(), hdrBytes, hdrBytes + sizeof(hdr));
    TestDDSHeaderDXT10 dx10{};
    dx10.dxgiFormat        = DXGI_FORMAT_R16G16B16A16_FLOAT;
    dx10.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    dx10.arraySize         = 1u;
    const auto* dx10Bytes = reinterpret_cast<const uint8_t*>(&dx10);
    buf.insert(buf.end(), dx10Bytes, dx10Bytes + sizeof(dx10));

    auto path = writeTempFile(buf, "_overflow.dds");
    EXPECT_THROW(ltc::DDSReader::readMetadata(path), ltc::DDSError);
    std::filesystem::remove(path);
}

// ============================================================================
// Pixel reading tests
// ============================================================================

TEST(DDSReaderTest, PixelReadCorrectAddressing) {
    // Create a 4x4 RGBA16F with known per-pixel values
    // Pixel (x,y) has channels: (x+1, y+1, 0, 1) encoded as fp16
    uint32_t w = 4, h = 4;
    // fp16 values: 0x3C00=1.0, 0x4000=2.0, 0x4200=3.0, 0x4400=4.0
    static const uint16_t fp16Vals[] = {0x0000u, 0x3C00u, 0x4000u, 0x4200u, 0x4400u};
    // Channels: R=x+1, G=y+1, B=0, A=1
    std::vector<std::array<uint16_t, 4>> pixels(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            pixels[y * w + x] = {fp16Vals[x + 1], fp16Vals[y + 1], 0x0000u, 0x3C00u};
        }
    }

    auto buf  = makeDDSDX10RGBA16F(w, h, pixels);
    auto path = writeTempFile(buf, "_pixread.dds");

    // Verify (0,0): R=1.0, G=1.0
    {
        ltc::Pixel p = ltc::DDSReader::readPixel(path, 0, 0);
        EXPECT_EQ(p.channelCount, 4u);
        EXPECT_FLOAT_EQ(p.channels[0], 1.0f);
        EXPECT_FLOAT_EQ(p.channels[1], 1.0f);
        EXPECT_FLOAT_EQ(p.channels[2], 0.0f);
        EXPECT_FLOAT_EQ(p.channels[3], 1.0f);
    }

    // Verify (3,0): R=4.0, G=1.0
    {
        ltc::Pixel p = ltc::DDSReader::readPixel(path, 3, 0);
        EXPECT_FLOAT_EQ(p.channels[0], 4.0f);
        EXPECT_FLOAT_EQ(p.channels[1], 1.0f);
    }

    // Verify (0,3): R=1.0, G=4.0
    {
        ltc::Pixel p = ltc::DDSReader::readPixel(path, 0, 3);
        EXPECT_FLOAT_EQ(p.channels[0], 1.0f);
        EXPECT_FLOAT_EQ(p.channels[1], 4.0f);
    }

    // Verify (3,3): R=4.0, G=4.0 (boundary pixel)
    {
        ltc::Pixel p = ltc::DDSReader::readPixel(path, 3, 3);
        EXPECT_FLOAT_EQ(p.channels[0], 4.0f);
        EXPECT_FLOAT_EQ(p.channels[1], 4.0f);
        EXPECT_FLOAT_EQ(p.channels[2], 0.0f);
        EXPECT_FLOAT_EQ(p.channels[3], 1.0f);
    }

    // Verify (2,1): R=3.0, G=2.0
    {
        ltc::Pixel p = ltc::DDSReader::readPixel(path, 2, 1);
        EXPECT_FLOAT_EQ(p.channels[0], 3.0f);
        EXPECT_FLOAT_EQ(p.channels[1], 2.0f);
    }

    std::filesystem::remove(path);
}

TEST(DDSReaderTest, PixelReadNegativeValues) {
    // -1.0f in fp16 = 0xBC00
    uint32_t w = 2, h = 2;
    std::vector<std::array<uint16_t, 4>> pixels = {
        {0xBC00u, 0xBC00u, 0xBC00u, 0xBC00u},
        {0x3C00u, 0xBC00u, 0x0000u, 0x3C00u},
        {0xBC00u, 0x3C00u, 0x0000u, 0x3C00u},
        {0x3C00u, 0x3C00u, 0x3C00u, 0x3C00u},
    };
    auto buf  = makeDDSDX10RGBA16F(w, h, pixels);
    auto path = writeTempFile(buf, "_neg.dds");

    ltc::Pixel p = ltc::DDSReader::readPixel(path, 0, 0);
    EXPECT_FLOAT_EQ(p.channels[0], -1.0f);
    EXPECT_FLOAT_EQ(p.channels[1], -1.0f);
    EXPECT_FLOAT_EQ(p.channels[2], -1.0f);
    EXPECT_FLOAT_EQ(p.channels[3], -1.0f);

    std::filesystem::remove(path);
}

TEST(DDSReaderTest, PixelReadOutOfRange) {
    uint32_t w = 4, h = 4;
    std::vector<std::array<uint16_t, 4>> pixels(w * h, {0x3C00u, 0x0000u, 0x0000u, 0x3C00u});
    auto buf  = makeDDSDX10RGBA16F(w, h, pixels);
    auto path = writeTempFile(buf, "_oob.dds");

    EXPECT_THROW(ltc::DDSReader::readPixel(path, 4, 0), ltc::DDSError);
    EXPECT_THROW(ltc::DDSReader::readPixel(path, 0, 4), ltc::DDSError);
    EXPECT_THROW(ltc::DDSReader::readPixel(path, 100, 100), ltc::DDSError);

    std::filesystem::remove(path);
}

// ============================================================================
// Stats tests
// ============================================================================

TEST(DDSReaderTest, ComputeStatsKnownValues) {
    // 2x2 RGBA16F: all pixels = (1.0, 2.0, 0.0, -1.0)
    uint32_t w = 2, h = 2;
    std::vector<std::array<uint16_t, 4>> pixels(w * h,
        {0x3C00u, 0x4000u, 0x0000u, 0xBC00u});
    auto buf  = makeDDSDX10RGBA16F(w, h, pixels);
    auto path = writeTempFile(buf, "_stats.dds");

    ltc::ImageStats ist = ltc::DDSReader::computeStats(path);
    EXPECT_EQ(ist.channelCount, 4u);

    // Channel 0: all 1.0
    EXPECT_TRUE(ist.channels[0].hasFinite);
    EXPECT_DOUBLE_EQ(ist.channels[0].min,  1.0);
    EXPECT_DOUBLE_EQ(ist.channels[0].max,  1.0);
    EXPECT_DOUBLE_EQ(ist.channels[0].mean, 1.0);

    // Channel 1: all 2.0
    EXPECT_TRUE(ist.channels[1].hasFinite);
    EXPECT_DOUBLE_EQ(ist.channels[1].min,  2.0);
    EXPECT_DOUBLE_EQ(ist.channels[1].max,  2.0);
    EXPECT_DOUBLE_EQ(ist.channels[1].mean, 2.0);

    // Channel 2: all 0.0
    EXPECT_TRUE(ist.channels[2].hasFinite);
    EXPECT_DOUBLE_EQ(ist.channels[2].min,  0.0);
    EXPECT_DOUBLE_EQ(ist.channels[2].max,  0.0);
    EXPECT_DOUBLE_EQ(ist.channels[2].mean, 0.0);

    // Channel 3: all -1.0
    EXPECT_TRUE(ist.channels[3].hasFinite);
    EXPECT_DOUBLE_EQ(ist.channels[3].min,  -1.0);
    EXPECT_DOUBLE_EQ(ist.channels[3].max,  -1.0);
    EXPECT_DOUBLE_EQ(ist.channels[3].mean, -1.0);

    // No special values
    EXPECT_EQ(ist.channels[0].nanCount,    0u);
    EXPECT_EQ(ist.channels[0].posInfCount, 0u);
    EXPECT_EQ(ist.channels[0].negInfCount, 0u);

    std::filesystem::remove(path);
}

TEST(DDSReaderTest, ComputeStatsSpecialValues) {
    // 2x1 RGBA16F: pixel0=(+Inf, NaN, 1.0, -Inf), pixel1=(2.0, NaN, 3.0, -Inf)
    // +Inf fp16 = 0x7C00, -Inf fp16 = 0xFC00, NaN fp16 = 0x7E00
    std::vector<std::array<uint16_t, 4>> pixels = {
        {0x7C00u, 0x7E00u, 0x3C00u, 0xFC00u},
        {0x4000u, 0x7E00u, 0x4200u, 0xFC00u},
    };
    auto buf  = makeDDSDX10RGBA16F(2u, 1u, pixels);
    auto path = writeTempFile(buf, "_special.dds");

    ltc::ImageStats ist = ltc::DDSReader::computeStats(path);

    // Channel 0: one +Inf, one 2.0
    EXPECT_EQ(ist.channels[0].posInfCount, 1u);
    EXPECT_TRUE(ist.channels[0].hasFinite);
    EXPECT_DOUBLE_EQ(ist.channels[0].min,  2.0);
    EXPECT_DOUBLE_EQ(ist.channels[0].max,  2.0);
    EXPECT_DOUBLE_EQ(ist.channels[0].mean, 2.0);

    // Channel 1: two NaNs
    EXPECT_EQ(ist.channels[1].nanCount, 2u);
    EXPECT_FALSE(ist.channels[1].hasFinite);

    // Channel 2: 1.0 and 3.0 → mean=2.0
    EXPECT_TRUE(ist.channels[2].hasFinite);
    EXPECT_DOUBLE_EQ(ist.channels[2].min,  1.0);
    EXPECT_DOUBLE_EQ(ist.channels[2].max,  3.0);
    EXPECT_DOUBLE_EQ(ist.channels[2].mean, 2.0);

    // Channel 3: two -Inf
    EXPECT_EQ(ist.channels[3].negInfCount, 2u);
    EXPECT_FALSE(ist.channels[3].hasFinite);

    std::filesystem::remove(path);
}

// ============================================================================
// Format helpers
// ============================================================================

TEST(DDSFormatTest, FormatToString) {
    EXPECT_EQ(ltc::formatToString(ltc::DDSFormat::RG16F),   "RG16F");
    EXPECT_EQ(ltc::formatToString(ltc::DDSFormat::RGBA16F), "RGBA16F");
    EXPECT_EQ(ltc::formatToString(ltc::DDSFormat::RG32F),   "RG32F");
    EXPECT_EQ(ltc::formatToString(ltc::DDSFormat::RGBA32F), "RGBA32F");
    EXPECT_EQ(ltc::formatToString(ltc::DDSFormat::Unknown), "Unknown");
}

TEST(DDSFormatTest, FormatChannelCount) {
    EXPECT_EQ(ltc::formatChannelCount(ltc::DDSFormat::RG16F),   2u);
    EXPECT_EQ(ltc::formatChannelCount(ltc::DDSFormat::RGBA16F), 4u);
    EXPECT_EQ(ltc::formatChannelCount(ltc::DDSFormat::RG32F),   2u);
    EXPECT_EQ(ltc::formatChannelCount(ltc::DDSFormat::RGBA32F), 4u);
}

TEST(DDSFormatTest, FormatBytesPerPixel) {
    EXPECT_EQ(ltc::formatBytesPerPixel(ltc::DDSFormat::RG16F),   4u);
    EXPECT_EQ(ltc::formatBytesPerPixel(ltc::DDSFormat::RGBA16F), 8u);
    EXPECT_EQ(ltc::formatBytesPerPixel(ltc::DDSFormat::RG32F),   8u);
    EXPECT_EQ(ltc::formatBytesPerPixel(ltc::DDSFormat::RGBA32F), 16u);
}
