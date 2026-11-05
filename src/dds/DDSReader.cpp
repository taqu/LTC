#include "ltc/DDSReader.h"
#include "ltc/DDS.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace ltc {

// ---- Format helpers --------------------------------------------------------

std::string formatToString(DDSFormat fmt) {
    switch (fmt) {
    case DDSFormat::RG16F:   return "RG16F";
    case DDSFormat::RGBA16F: return "RGBA16F";
    case DDSFormat::RG32F:   return "RG32F";
    case DDSFormat::RGBA32F: return "RGBA32F";
    default:                 return "Unknown";
    }
}

uint32_t formatChannelCount(DDSFormat fmt) {
    switch (fmt) {
    case DDSFormat::RG16F:   return 2;
    case DDSFormat::RGBA16F: return 4;
    case DDSFormat::RG32F:   return 2;
    case DDSFormat::RGBA32F: return 4;
    default:                 return 0;
    }
}

uint32_t formatBytesPerPixel(DDSFormat fmt) {
    switch (fmt) {
    case DDSFormat::RG16F:   return 4;   // 2 × 2 bytes
    case DDSFormat::RGBA16F: return 8;   // 4 × 2 bytes
    case DDSFormat::RG32F:   return 8;   // 2 × 4 bytes
    case DDSFormat::RGBA32F: return 16;  // 4 × 4 bytes
    default:                 return 0;
    }
}

// ---- DDS binary layout constants -------------------------------------------

static constexpr uint32_t DDS_MAGIC                             = 0x20534444u;
static constexpr uint32_t DDPF_FOURCC                           = 0x4u;
static constexpr uint32_t FOURCC_DX10                           = 0x30315844u;
static constexpr uint32_t D3D10_RESOURCE_DIMENSION_TEXTURE2D    = 3u;

// Legacy FourCC float formats
static constexpr uint32_t D3DFMT_G16R16F       = 112u;
static constexpr uint32_t D3DFMT_A16B16G16R16F = 113u;
static constexpr uint32_t D3DFMT_G32R32F       = 115u;
static constexpr uint32_t D3DFMT_A32B32G32R32F = 116u;

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

// ---- Internal helper: read and parse header --------------------------------

struct ParsedHeader {
    DDSMetadata meta;
    bool hasDX10;
};

static ParsedHeader parseHeader(std::ifstream& f, const std::filesystem::path& path) {
    // Determine file size
    f.seekg(0, std::ios::end);
    if (!f) {
        throw DDSError("failed to seek in file: " + path.string());
    }
    auto fileSize = static_cast<uint64_t>(f.tellg());
    f.seekg(0, std::ios::beg);

    // Need at least: magic(4) + header(124) = 128 bytes
    if (fileSize < 128u) {
        throw DDSError("file too small to contain DDS header: " + path.string());
    }

    // Read magic
    uint32_t magic = 0;
    f.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (!f || magic != DDS_MAGIC) {
        throw DDSError("invalid DDS magic number: " + path.string());
    }

    // Read header
    DDSHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!f) {
        throw DDSError("failed to read DDS header: " + path.string());
    }

    if (hdr.dwSize != 124u) {
        throw DDSError("invalid DDS header size (expected 124, got " +
                       std::to_string(hdr.dwSize) + "): " + path.string());
    }
    if (hdr.ddspf.dwSize != 32u) {
        throw DDSError("invalid DDS pixel format size (expected 32, got " +
                       std::to_string(hdr.ddspf.dwSize) + "): " + path.string());
    }
    if (hdr.dwWidth == 0u || hdr.dwHeight == 0u) {
        throw DDSError("invalid DDS dimensions (width=" + std::to_string(hdr.dwWidth) +
                       " height=" + std::to_string(hdr.dwHeight) + "): " + path.string());
    }

    bool hasDX10 = (hdr.ddspf.dwFlags & DDPF_FOURCC) != 0u &&
                   hdr.ddspf.dwFourCC == FOURCC_DX10;

    DDSHeaderDXT10 dx10{};
    if (hasDX10) {
        // Need additional 20 bytes
        if (fileSize < 148u) {
            throw DDSError("file too small to contain DX10 extended header: " + path.string());
        }
        f.read(reinterpret_cast<char*>(&dx10), sizeof(dx10));
        if (!f) {
            throw DDSError("failed to read DX10 extended header: " + path.string());
        }
        if (dx10.resourceDimension != D3D10_RESOURCE_DIMENSION_TEXTURE2D) {
            if (dx10.resourceDimension == 4u) {
                throw DDSError("3D textures are not supported: " + path.string());
            } else if (dx10.resourceDimension == 2u) {
                throw DDSError("1D textures are not supported: " + path.string());
            } else {
                throw DDSError("unsupported resource dimension (" +
                               std::to_string(dx10.resourceDimension) + "): " + path.string());
            }
        }
        if (dx10.arraySize > 1u) {
            throw DDSError("array textures are not supported: " + path.string());
        }
    }

    // Determine format
    DDSFormat fmt = DDSFormat::Unknown;
    if (hasDX10) {
        switch (dx10.dxgiFormat) {
        case DXGI_FORMAT_R32G32B32A32_FLOAT: fmt = DDSFormat::RGBA32F; break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT: fmt = DDSFormat::RGBA16F; break;
        case DXGI_FORMAT_R32G32_FLOAT:       fmt = DDSFormat::RG32F;   break;
        case DXGI_FORMAT_R16G16_FLOAT:       fmt = DDSFormat::RG16F;   break;
        default:
            throw DDSError("unsupported DXGI format (" +
                           std::to_string(dx10.dxgiFormat) + "): " + path.string());
        }
    } else {
        if ((hdr.ddspf.dwFlags & DDPF_FOURCC) != 0u) {
            switch (hdr.ddspf.dwFourCC) {
            case D3DFMT_G16R16F:       fmt = DDSFormat::RG16F;   break;
            case D3DFMT_A16B16G16R16F: fmt = DDSFormat::RGBA16F; break;
            case D3DFMT_G32R32F:       fmt = DDSFormat::RG32F;   break;
            case D3DFMT_A32B32G32R32F: fmt = DDSFormat::RGBA32F; break;
            default:
                throw DDSError("unsupported legacy FourCC (" +
                               std::to_string(hdr.ddspf.dwFourCC) + "): " + path.string());
            }
        } else {
            throw DDSError("unsupported DDS pixel format (no FOURCC): " + path.string());
        }
    }

    uint32_t channelCount  = formatChannelCount(fmt);
    uint32_t bytesPerPixel = formatBytesPerPixel(fmt);
    uint64_t dataOffset    = hasDX10 ? 148u : 128u;

    // Check integer overflow before computing dataSize
    if (hdr.dwWidth > std::numeric_limits<uint64_t>::max() / bytesPerPixel) {
        throw DDSError("image dimensions overflow: " + path.string());
    }
    uint64_t rowBytes = static_cast<uint64_t>(hdr.dwWidth) * bytesPerPixel;
    if (hdr.dwHeight > std::numeric_limits<uint64_t>::max() / rowBytes) {
        throw DDSError("image dimensions overflow: " + path.string());
    }
    uint64_t dataSize = rowBytes * static_cast<uint64_t>(hdr.dwHeight);

    if (fileSize < dataOffset + dataSize) {
        throw DDSError("file too small for declared image data (need " +
                       std::to_string(dataOffset + dataSize) + " bytes, have " +
                       std::to_string(fileSize) + "): " + path.string());
    }

    DDSMetadata meta{};
    meta.width         = hdr.dwWidth;
    meta.height        = hdr.dwHeight;
    meta.depth         = (hdr.dwDepth > 0u) ? hdr.dwDepth : 1u;
    meta.mipLevels     = (hdr.dwMipMapCount > 0u) ? hdr.dwMipMapCount : 1u;
    meta.arraySize     = hasDX10 ? dx10.arraySize : 1u;
    meta.format        = fmt;
    meta.channelCount  = channelCount;
    meta.bytesPerPixel = bytesPerPixel;
    meta.dataOffset    = dataOffset;
    meta.dataSize      = dataSize;

    return ParsedHeader{meta, hasDX10};
}

// ---- Public API ------------------------------------------------------------

DDSMetadata DDSReader::readMetadata(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw DDSError("cannot open file: " + path.string());
    }
    return parseHeader(f, path).meta;
}

Pixel DDSReader::readPixel(const std::filesystem::path& path, uint32_t x, uint32_t y) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw DDSError("cannot open file: " + path.string());
    }

    auto [meta, hasDX10] = parseHeader(f, path);
    (void)hasDX10;

    if (x >= meta.width || y >= meta.height) {
        throw DDSError("coordinates out of range (" + std::to_string(x) + "," +
                       std::to_string(y) + ") for image " + std::to_string(meta.width) +
                       "x" + std::to_string(meta.height) + ": " + path.string());
    }
    if (meta.arraySize != 1u) {
        throw DDSError("array textures are not supported for pixel access: " + path.string());
    }

    uint64_t pixelOffset = meta.dataOffset +
                           (static_cast<uint64_t>(y) * meta.width + x) * meta.bytesPerPixel;

    f.seekg(static_cast<std::streamoff>(pixelOffset), std::ios::beg);
    if (!f) {
        throw DDSError("failed to seek to pixel data: " + path.string());
    }

    // Read raw bytes
    uint8_t buf[16]{};
    f.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(meta.bytesPerPixel));
    if (!f) {
        throw DDSError("failed to read pixel data: " + path.string());
    }

    Pixel pix{};
    pix.channelCount = meta.channelCount;

    switch (meta.format) {
    case DDSFormat::RG16F: {
        uint16_t r16 = 0, g16 = 0;
        std::memcpy(&r16, buf + 0, 2);
        std::memcpy(&g16, buf + 2, 2);
        pix.channels[0] = fp16ToFloat(r16);
        pix.channels[1] = fp16ToFloat(g16);
        break;
    }
    case DDSFormat::RGBA16F: {
        uint16_t r16 = 0, g16 = 0, b16 = 0, a16 = 0;
        std::memcpy(&r16, buf + 0, 2);
        std::memcpy(&g16, buf + 2, 2);
        std::memcpy(&b16, buf + 4, 2);
        std::memcpy(&a16, buf + 6, 2);
        pix.channels[0] = fp16ToFloat(r16);
        pix.channels[1] = fp16ToFloat(g16);
        pix.channels[2] = fp16ToFloat(b16);
        pix.channels[3] = fp16ToFloat(a16);
        break;
    }
    case DDSFormat::RG32F: {
        float r = 0.0f, g = 0.0f;
        std::memcpy(&r, buf + 0, 4);
        std::memcpy(&g, buf + 4, 4);
        pix.channels[0] = r;
        pix.channels[1] = g;
        break;
    }
    case DDSFormat::RGBA32F: {
        float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
        std::memcpy(&r, buf + 0,  4);
        std::memcpy(&g, buf + 4,  4);
        std::memcpy(&b, buf + 8,  4);
        std::memcpy(&a, buf + 12, 4);
        pix.channels[0] = r;
        pix.channels[1] = g;
        pix.channels[2] = b;
        pix.channels[3] = a;
        break;
    }
    default:
        throw DDSError("unsupported format for pixel read: " + path.string());
    }

    return pix;
}

ImageStats DDSReader::computeStats(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw DDSError("cannot open file: " + path.string());
    }

    auto [meta, hasDX10] = parseHeader(f, path);
    (void)hasDX10;

    uint32_t nCh = meta.channelCount;

    // Accumulators
    std::array<double, 4> sum{};
    std::array<double, 4> minVal{};
    std::array<double, 4> maxVal{};
    std::array<uint64_t, 4> nanCount{};
    std::array<uint64_t, 4> posInfCount{};
    std::array<uint64_t, 4> negInfCount{};
    std::array<uint64_t, 4> finiteCount{};

    for (uint32_t c = 0; c < 4; ++c) {
        minVal[c]    = std::numeric_limits<double>::max();
        maxVal[c]    = std::numeric_limits<double>::lowest();
    }

    f.seekg(static_cast<std::streamoff>(meta.dataOffset), std::ios::beg);
    if (!f) {
        throw DDSError("failed to seek to image data: " + path.string());
    }

    // Read row by row to avoid huge single allocations on large files
    std::vector<uint8_t> rowBuf(static_cast<size_t>(meta.width) * meta.bytesPerPixel);

    for (uint32_t row = 0; row < meta.height; ++row) {
        f.read(reinterpret_cast<char*>(rowBuf.data()),
               static_cast<std::streamsize>(rowBuf.size()));
        if (!f) {
            throw DDSError("failed to read image row " + std::to_string(row) +
                           ": " + path.string());
        }

        for (uint32_t col = 0; col < meta.width; ++col) {
            const uint8_t* p = rowBuf.data() + static_cast<size_t>(col) * meta.bytesPerPixel;
            float vals[4]{};

            switch (meta.format) {
            case DDSFormat::RG16F: {
                uint16_t r16 = 0, g16 = 0;
                std::memcpy(&r16, p + 0, 2);
                std::memcpy(&g16, p + 2, 2);
                vals[0] = fp16ToFloat(r16);
                vals[1] = fp16ToFloat(g16);
                break;
            }
            case DDSFormat::RGBA16F: {
                uint16_t r16 = 0, g16 = 0, b16 = 0, a16 = 0;
                std::memcpy(&r16, p + 0, 2);
                std::memcpy(&g16, p + 2, 2);
                std::memcpy(&b16, p + 4, 2);
                std::memcpy(&a16, p + 6, 2);
                vals[0] = fp16ToFloat(r16);
                vals[1] = fp16ToFloat(g16);
                vals[2] = fp16ToFloat(b16);
                vals[3] = fp16ToFloat(a16);
                break;
            }
            case DDSFormat::RG32F:
                std::memcpy(&vals[0], p + 0, 4);
                std::memcpy(&vals[1], p + 4, 4);
                break;
            case DDSFormat::RGBA32F:
                std::memcpy(&vals[0], p + 0,  4);
                std::memcpy(&vals[1], p + 4,  4);
                std::memcpy(&vals[2], p + 8,  4);
                std::memcpy(&vals[3], p + 12, 4);
                break;
            default:
                break;
            }

            for (uint32_t c = 0; c < nCh; ++c) {
                double v = static_cast<double>(vals[c]);
                if (std::isnan(v)) {
                    ++nanCount[c];
                } else if (std::isinf(v)) {
                    if (v > 0.0) ++posInfCount[c];
                    else         ++negInfCount[c];
                } else {
                    if (v < minVal[c]) minVal[c] = v;
                    if (v > maxVal[c]) maxVal[c] = v;
                    sum[c] += v;
                    ++finiteCount[c];
                }
            }
        }
    }

    ImageStats stats{};
    stats.channelCount = nCh;
    for (uint32_t c = 0; c < nCh; ++c) {
        ChannelStats& cs = stats.channels[c];
        cs.nanCount    = nanCount[c];
        cs.posInfCount = posInfCount[c];
        cs.negInfCount = negInfCount[c];
        if (finiteCount[c] > 0u) {
            cs.hasFinite = true;
            cs.min  = minVal[c];
            cs.max  = maxVal[c];
            cs.mean = sum[c] / static_cast<double>(finiteCount[c]);
        }
    }

    return stats;
}

// ---- fp16 → fp32 conversion ------------------------------------------------

float DDSReader::fp16ToFloat(uint16_t h) noexcept {
    uint32_t sign = static_cast<uint32_t>(h >> 15u) << 31u;
    uint32_t expo = (h >> 10u) & 0x1Fu;
    uint32_t mant = h & 0x3FFu;
    uint32_t bits = 0u;

    if (expo == 0u) {
        if (mant == 0u) {
            // Signed zero
            bits = sign;
        } else {
            // Subnormal fp16: value = sign * 2^(-14) * mant / 1024
            // Normalize: shift mant left until bit 10 is set, counting shifts
            int32_t shift = 0;
            uint32_t m = mant;
            while ((m & 0x400u) == 0u) {
                m <<= 1u;
                ++shift;
            }
            // fp32 biased exponent: (-14 - shift) + 127 = 113 - shift
            uint32_t fp32exp = static_cast<uint32_t>(113 - shift);
            bits = sign | (fp32exp << 23u) | ((m & 0x3FFu) << 13u);
        }
    } else if (expo == 0x1Fu) {
        // Infinity or NaN
        bits = sign | 0x7F800000u | (mant << 13u);
    } else {
        // Normal number: fp32 exp = expo + (127 - 15) = expo + 112
        bits = sign | ((expo + 112u) << 23u) | (mant << 13u);
    }

    float result;
    std::memcpy(&result, &bits, sizeof(float));
    return result;
}

} // namespace ltc
