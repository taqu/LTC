#include "ltc/DDSWriter.h"
#include "ltc/TextureData.h"
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

namespace ltc {

// ---- Format helpers --------------------------------------------------------

uint32_t channelCount(TextureFormat fmt) {
    switch (fmt) {
    case TextureFormat::RG16F:   return 2;
    case TextureFormat::RGBA16F: return 4;
    case TextureFormat::RG32F:   return 2;
    case TextureFormat::RGBA32F: return 4;
    }
    return 0;
}

uint32_t bytesPerPixel(TextureFormat fmt) {
    switch (fmt) {
    case TextureFormat::RG16F:   return 4;   // 2 × 2 bytes
    case TextureFormat::RGBA16F: return 8;   // 4 × 2 bytes
    case TextureFormat::RG32F:   return 8;   // 2 × 4 bytes
    case TextureFormat::RGBA32F: return 16;  // 4 × 4 bytes
    }
    return 0;
}

// ---- DDS binary layout constants -------------------------------------------

static constexpr uint32_t DDS_MAGIC                             = 0x20534444u;
static constexpr uint32_t DDSD_CAPS                             = 0x1u;
static constexpr uint32_t DDSD_HEIGHT                           = 0x2u;
static constexpr uint32_t DDSD_WIDTH                            = 0x4u;
static constexpr uint32_t DDSD_PIXELFORMAT                      = 0x1000u;
static constexpr uint32_t DDSD_LINEARSIZE                       = 0x80000u;
static constexpr uint32_t DDPF_FOURCC                           = 0x4u;
static constexpr uint32_t DDSCAPS_TEXTURE                       = 0x1000u;
static constexpr uint32_t FOURCC_DX10                           = 0x30315844u;
static constexpr uint32_t D3D10_RESOURCE_DIMENSION_TEXTURE2D    = 3u;

// DXGI format values
static constexpr uint32_t DXGI_FORMAT_R32G32B32A32_FLOAT = 2u;
static constexpr uint32_t DXGI_FORMAT_R16G16B16A16_FLOAT = 10u;
static constexpr uint32_t DXGI_FORMAT_R32G32_FLOAT       = 16u;
static constexpr uint32_t DXGI_FORMAT_R16G16_FLOAT       = 34u;

// ---- Packed DDS structs ----------------------------------------------------

#pragma pack(push, 1)
struct DDSPixelFormat {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
};

struct DDSHeader {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    uint32_t dwPitchOrLinearSize;
    uint32_t dwDepth;
    uint32_t dwMipMapCount;
    uint32_t dwReserved1[11];
    DDSPixelFormat ddspf;
    uint32_t dwCaps;
    uint32_t dwCaps2;
    uint32_t dwCaps3;
    uint32_t dwCaps4;
    uint32_t dwReserved2;
};

struct DDSHeaderDXT10 {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};
#pragma pack(pop)

static_assert(sizeof(DDSPixelFormat) == 32, "DDSPixelFormat must be 32 bytes");
static_assert(sizeof(DDSHeader) == 124, "DDSHeader must be 124 bytes");
static_assert(sizeof(DDSHeaderDXT10) == 20, "DDSHeaderDXT10 must be 20 bytes");

static uint32_t dxgiFormatFor(TextureFormat fmt) {
    switch (fmt) {
    case TextureFormat::RG16F:   return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::RGBA16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::RG32F:   return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::RGBA32F: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    return 0;
}

void DDSWriter::write(const std::filesystem::path& path, const TextureData& texture) {
    if (texture.width == 0 || texture.height == 0) {
        throw std::runtime_error("DDSWriter: invalid dimensions (" +
            std::to_string(texture.width) + "x" + std::to_string(texture.height) + ")");
    }

    uint32_t bpp       = bytesPerPixel(texture.format);
    uint64_t dataSize  = static_cast<uint64_t>(texture.width) * texture.height * bpp;

    if (texture.pixels.size() != dataSize) {
        throw std::runtime_error("DDSWriter: pixel data size mismatch: expected " +
            std::to_string(dataSize) + " bytes, got " +
            std::to_string(texture.pixels.size()) + " bytes");
    }

    // Build DDS_PIXELFORMAT
    DDSPixelFormat ddspf{};
    ddspf.dwSize   = 32;
    ddspf.dwFlags  = DDPF_FOURCC;
    ddspf.dwFourCC = FOURCC_DX10;

    // Build DDS_HEADER
    DDSHeader hdr{};
    hdr.dwSize             = 124;
    hdr.dwFlags            = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE;
    hdr.dwHeight           = texture.height;
    hdr.dwWidth            = texture.width;
    hdr.dwPitchOrLinearSize = texture.width * bpp; // pitch (bytes per row)
    hdr.dwDepth            = 1;
    hdr.dwMipMapCount      = 1;
    std::memset(hdr.dwReserved1, 0, sizeof(hdr.dwReserved1));
    hdr.ddspf              = ddspf;
    hdr.dwCaps             = DDSCAPS_TEXTURE;
    hdr.dwCaps2            = 0;
    hdr.dwCaps3            = 0;
    hdr.dwCaps4            = 0;
    hdr.dwReserved2        = 0;

    // Build DDS_HEADER_DXT10
    DDSHeaderDXT10 dx10{};
    dx10.dxgiFormat        = dxgiFormatFor(texture.format);
    dx10.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    dx10.miscFlag          = 0;
    dx10.arraySize         = 1;
    dx10.miscFlags2        = 0;

    // Write to temp file, then rename
    std::filesystem::path tmpPath = path;
    tmpPath += ".tmp";

    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("DDSWriter: cannot open temp file for writing: " +
                tmpPath.string());
        }

        // Magic
        uint32_t magic = DDS_MAGIC;
        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));

        // DDS header (124 bytes)
        out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

        // DX10 extended header (20 bytes)
        out.write(reinterpret_cast<const char*>(&dx10), sizeof(dx10));

        // Pixel data
        out.write(reinterpret_cast<const char*>(texture.pixels.data()),
                  static_cast<std::streamsize>(dataSize));

        if (!out) {
            throw std::runtime_error("DDSWriter: write failed: " + tmpPath.string());
        }
    }

    // Rename temp file to target
    std::filesystem::rename(tmpPath, path);
}

} // namespace ltc
