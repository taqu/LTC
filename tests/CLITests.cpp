#include <gtest/gtest.h>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#endif

// ---- Subprocess helper -----------------------------------------------------

static std::string runCommand(const std::string& cmd, int& exitCode) {
    std::string output;
#ifdef _WIN32
    // cmd.exe /c strips the first and last quote from a string that starts with '"'.
    // Wrap the whole command in an extra pair of outer quotes so cmd.exe sees the
    // intended quoted exe path and quoted argument path after stripping.
    FILE* pipe = _popen(("\"" + cmd + "\"").c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        exitCode = -1;
        return "";
    }
    std::array<char, 256> buf{};
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
        output += buf.data();
    }
#ifdef _WIN32
    exitCode = _pclose(pipe);
#else
    int status = pclose(pipe);
    exitCode = WEXITSTATUS(status);
#endif
    return output;
}

// ---- Synthetic DDS builder (minimal, same as DDSReaderTests) ---------------

static constexpr uint32_t CLI_DDS_MAGIC   = 0x20534444u;
static constexpr uint32_t CLI_DDPF_FOURCC = 0x4u;
static constexpr uint32_t CLI_FOURCC_DX10 = 0x30315844u;
static constexpr uint32_t CLI_DXGI_RGBA16F = 10u;
static constexpr uint32_t CLI_RES_DIM_TEX2D = 3u;

#pragma pack(push, 1)
struct CliDDSPixelFormat {
    uint32_t dwSize        = 32u;
    uint32_t dwFlags       = 0u;
    uint32_t dwFourCC      = 0u;
    uint32_t dwRGBBitCount = 0u;
    uint32_t dwRBitMask    = 0u;
    uint32_t dwGBitMask    = 0u;
    uint32_t dwBBitMask    = 0u;
    uint32_t dwABitMask    = 0u;
};
static_assert(sizeof(CliDDSPixelFormat) == 32);

struct CliDDSHeader {
    uint32_t dwSize              = 124u;
    uint32_t dwFlags             = 0x1007u;
    uint32_t dwHeight            = 0u;
    uint32_t dwWidth             = 0u;
    uint32_t dwPitchOrLinearSize = 0u;
    uint32_t dwDepth             = 0u;
    uint32_t dwMipMapCount       = 1u;
    uint32_t dwReserved1[11]     = {};
    CliDDSPixelFormat ddspf;
    uint32_t dwCaps              = 0x1000u;
    uint32_t dwCaps2             = 0u;
    uint32_t dwCaps3             = 0u;
    uint32_t dwCaps4             = 0u;
    uint32_t dwReserved2         = 0u;
};
static_assert(sizeof(CliDDSHeader) == 124);

struct CliDDSHeaderDXT10 {
    uint32_t dxgiFormat        = 0u;
    uint32_t resourceDimension = CLI_RES_DIM_TEX2D;
    uint32_t miscFlag          = 0u;
    uint32_t arraySize         = 1u;
    uint32_t miscFlags2        = 0u;
};
static_assert(sizeof(CliDDSHeaderDXT10) == 20);
#pragma pack(pop)

static std::filesystem::path writeCliTestDDS(uint32_t w, uint32_t h,
    const std::vector<std::array<uint16_t, 4>>& pixels,
    const std::string& tag = "")
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

    writeU32(CLI_DDS_MAGIC);
    CliDDSHeader hdr{};
    hdr.dwWidth  = w;
    hdr.dwHeight = h;
    hdr.ddspf.dwFlags  = CLI_DDPF_FOURCC;
    hdr.ddspf.dwFourCC = CLI_FOURCC_DX10;
    writeBytes(&hdr, sizeof(hdr));
    CliDDSHeaderDXT10 dx10{};
    dx10.dxgiFormat        = CLI_DXGI_RGBA16F;
    dx10.resourceDimension = CLI_RES_DIM_TEX2D;
    dx10.arraySize         = 1u;
    writeBytes(&dx10, sizeof(dx10));
    for (auto& px : pixels) {
        writeU16(px[0]);
        writeU16(px[1]);
        writeU16(px[2]);
        writeU16(px[3]);
    }

    auto path = std::filesystem::temp_directory_path() /
                ("ltc_cli_test" + tag + ".dds");
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(buf.data()),
            static_cast<std::streamsize>(buf.size()));
    return path;
}

// ---- Quoted path helper (Windows needs quoted paths with spaces) ------------
static std::string quotePath(const std::filesystem::path& p) {
    return "\"" + p.string() + "\"";
}

static std::string exePath() {
    return std::string("\"") + LTCGEN_EXECUTABLE + "\"";
}

// ---- Tests -----------------------------------------------------------------

TEST(CLITest, Help) {
    int code = 0;
    auto out = runCommand(exePath() + " --help", code);
    EXPECT_EQ(code, 0);
    EXPECT_NE(out.find("inspect"), std::string::npos);
}

TEST(CLITest, Version) {
    int code = 0;
    auto out = runCommand(exePath() + " --version", code);
    EXPECT_EQ(code, 0);
    EXPECT_NE(out.find("0.1.0"), std::string::npos);
}

TEST(CLITest, InspectMetadata) {
    uint32_t w = 4, h = 4;
    std::vector<std::array<uint16_t, 4>> pixels(w * h,
        {0x3C00u, 0x4000u, 0x0000u, 0x3C00u});
    auto path = writeCliTestDDS(w, h, pixels, "_meta");

    int code = 0;
    auto out = runCommand(exePath() + " inspect " + quotePath(path), code);
    EXPECT_EQ(code, 0) << "stdout: " << out;
    EXPECT_NE(out.find("RGBA16F"), std::string::npos);
    EXPECT_NE(out.find("4"), std::string::npos); // width/height

    std::filesystem::remove(path);
}

TEST(CLITest, InspectMetadataJson) {
    uint32_t w = 2, h = 2;
    std::vector<std::array<uint16_t, 4>> pixels(w * h,
        {0x3C00u, 0x0000u, 0x0000u, 0x3C00u});
    auto path = writeCliTestDDS(w, h, pixels, "_json");

    int code = 0;
    auto out = runCommand(exePath() + " inspect " + quotePath(path) + " --json", code);
    EXPECT_EQ(code, 0) << "stdout: " << out;
    EXPECT_NE(out.find("\"format\""), std::string::npos);
    EXPECT_NE(out.find("RGBA16F"), std::string::npos);
    EXPECT_NE(out.find("\"width\""), std::string::npos);
    // Verify it starts with '{' (is JSON)
    auto trimmed = out;
    auto pos = trimmed.find_first_not_of(" \t\r\n");
    ASSERT_NE(pos, std::string::npos);
    EXPECT_EQ(trimmed[pos], '{');

    std::filesystem::remove(path);
}

TEST(CLITest, InspectPixel) {
    uint32_t w = 4, h = 4;
    // Set pixel (2,1) to (1.0, 2.0, 0.0, 1.0)
    std::vector<std::array<uint16_t, 4>> pixels(w * h,
        {0x0000u, 0x0000u, 0x0000u, 0x0000u});
    pixels[1 * w + 2] = {0x3C00u, 0x4000u, 0x0000u, 0x3C00u};
    auto path = writeCliTestDDS(w, h, pixels, "_pix");

    int code = 0;
    auto out = runCommand(exePath() + " inspect " + quotePath(path) + " --pixel 2,1", code);
    EXPECT_EQ(code, 0) << "stdout: " << out;
    EXPECT_NE(out.find("1"), std::string::npos);  // some channel output

    std::filesystem::remove(path);
}

TEST(CLITest, InspectStats) {
    uint32_t w = 2, h = 2;
    std::vector<std::array<uint16_t, 4>> pixels(w * h,
        {0x3C00u, 0x4000u, 0x0000u, 0xBC00u});
    auto path = writeCliTestDDS(w, h, pixels, "_stats");

    int code = 0;
    auto out = runCommand(exePath() + " inspect " + quotePath(path) + " --stats", code);
    EXPECT_EQ(code, 0) << "stdout: " << out;
    EXPECT_NE(out.find("Channel"), std::string::npos);

    std::filesystem::remove(path);
}

TEST(CLITest, InspectStatsJson) {
    uint32_t w = 2, h = 2;
    std::vector<std::array<uint16_t, 4>> pixels(w * h,
        {0x3C00u, 0x4000u, 0x0000u, 0xBC00u});
    auto path = writeCliTestDDS(w, h, pixels, "_statsjson");

    int code = 0;
    auto out = runCommand(exePath() + " inspect " + quotePath(path) + " --json --stats", code);
    EXPECT_EQ(code, 0) << "stdout: " << out;
    EXPECT_NE(out.find("\"stats\""), std::string::npos);
    EXPECT_NE(out.find("\"min\""), std::string::npos);
    EXPECT_NE(out.find("\"mean\""), std::string::npos);

    std::filesystem::remove(path);
}

TEST(CLITest, InspectNonexistentFile) {
    int code = 0;
    runCommand(exePath() + " inspect /nonexistent/path/file.dds 2>&1", code);
    EXPECT_NE(code, 0);
}

TEST(CLITest, InspectInvalidPixelCoord) {
    uint32_t w = 4, h = 4;
    std::vector<std::array<uint16_t, 4>> pixels(w * h,
        {0x3C00u, 0x0000u, 0x0000u, 0x3C00u});
    auto path = writeCliTestDDS(w, h, pixels, "_badpix");

    // Out-of-range pixel — exits non-zero
    int code = 0;
    runCommand(exePath() + " inspect " + quotePath(path) + " --pixel 100,100 2>&1", code);
    EXPECT_NE(code, 0);

    std::filesystem::remove(path);
}
