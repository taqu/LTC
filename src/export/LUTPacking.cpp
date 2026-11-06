#include "ltc/LUTPacking.h"
#include "ltc/Float16.h"
#include "ltc/Math.h"
#include "ltc/ParameterGrid.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace ltc {

PackedLTCMatrix packLTCMatrix(const Mat3& matrix) {
    // Compute inverse of the input matrix
    auto invOpt = safeInverse(matrix);
    if (!invOpt) {
        throw std::runtime_error("packLTCMatrix: singular matrix, cannot invert");
    }
    Mat3 invM = *invOpt;

    // Validate all elements are finite
    if (!isFinite(invM)) {
        throw std::runtime_error("packLTCMatrix: inverse matrix contains non-finite values");
    }

    // Normalize by center diagonal element: invM[col=1][row=1]
    double denom = invM[1][1];
    if (std::abs(denom) < 1e-10) {
        throw std::runtime_error("packLTCMatrix: center diagonal element is too small (degenerate matrix)");
    }

    // Divide all elements by denom
    invM /= denom;

    // Validate after normalization
    if (!isFinite(invM)) {
        throw std::runtime_error("packLTCMatrix: normalized inverse matrix contains non-finite values");
    }

    // Extract 4 packed values (GLM column-major: [col][row])
    // r = invM[col=0][row=0], g = invM[col=0][row=2]
    // b = invM[col=2][row=0], a = invM[col=2][row=2]
    PackedLTCMatrix result;
    result.r = invM[0][0];
    result.g = invM[0][2];
    result.b = invM[2][0];
    result.a = invM[2][2];

    return result;
}

double computeSphericalCapCoefficient(double ndotv) {
    // Clamp ndotv to [0, 1]
    if (ndotv < 0.0) ndotv = 0.0;
    if (ndotv > 1.0) ndotv = 1.0;

    // Spherical cap coefficient: 2*pi*(1 - ndotv)
    // At ndotv=1 (straight up): 0
    // At ndotv=0 (grazing): 2*pi
    return kTwoPi * (1.0 - ndotv);
}

// Helper: encode a double value as bytes in the texture
static void encodeChannel(std::byte* dst, double value, TextureFormat fmt, uint32_t channelOffset) {
    if (fmt == TextureFormat::RG16F || fmt == TextureFormat::RGBA16F) {
        uint16_t h = float32ToFloat16(static_cast<float>(value));
        std::memcpy(dst + channelOffset * 2, &h, sizeof(h));
    } else {
        // FP32
        float f = static_cast<float>(value);
        std::memcpy(dst + channelOffset * 4, &f, sizeof(f));
    }
}

TextureData buildMatrixTexture(const LUTData& lut, ExportProfile /*profile*/, TextureFormat fmt) {
    uint32_t w = lut.width();
    uint32_t h = lut.height();

    if (w == 0 || h == 0) {
        throw std::runtime_error("buildMatrixTexture: LUT has zero dimensions");
    }

    // Matrix texture always has 4 channels (RGBA)
    // Validate format has 4 channels
    if (channelCount(fmt) != 4) {
        throw std::runtime_error("buildMatrixTexture: matrix texture requires 4-channel format (RGBA16F or RGBA32F)");
    }

    TextureData tex;
    tex.width  = w;
    tex.height = h;
    tex.format = fmt;
    tex.pixels.resize(static_cast<size_t>(w) * h * bytesPerPixel(fmt), std::byte{0});

    uint32_t bpp = bytesPerPixel(fmt);

    for (uint32_t vi = 0; vi < h; ++vi) {
        for (uint32_t ri = 0; ri < w; ++ri) {
            const LUTCell& cell = lut.at(ri, vi);

            if (!cell.completed) {
                throw std::runtime_error("buildMatrixTexture: cell (" +
                    std::to_string(ri) + "," + std::to_string(vi) + ") is not completed");
            }

            // Pack the LTC matrix (inverts it internally)
            PackedLTCMatrix packed = packLTCMatrix(cell.matrix);

            // pixelIndex = vi * width + ri
            size_t pixelIndex = static_cast<size_t>(vi) * w + ri;
            std::byte* dst = tex.pixels.data() + pixelIndex * bpp;

            encodeChannel(dst, packed.r, fmt, 0);
            encodeChannel(dst, packed.g, fmt, 1);
            encodeChannel(dst, packed.b, fmt, 2);
            encodeChannel(dst, packed.a, fmt, 3);
        }
    }

    return tex;
}

TextureData buildAmplitudeTexture(const LUTData& lut, ExportProfile profile, TextureFormat fmt) {
    uint32_t w = lut.width();
    uint32_t h = lut.height();

    if (w == 0 || h == 0) {
        throw std::runtime_error("buildAmplitudeTexture: LUT has zero dimensions");
    }

    // For Reference profile, force RGBA16F (4 channels needed for spherical cap)
    // For Unreal profile, use the provided format (typically RG16F)
    if (profile == ExportProfile::Reference) {
        fmt = TextureFormat::RGBA16F;
    }

    TextureData tex;
    tex.width  = w;
    tex.height = h;
    tex.format = fmt;
    tex.pixels.resize(static_cast<size_t>(w) * h * bytesPerPixel(fmt), std::byte{0});

    uint32_t bpp = bytesPerPixel(fmt);
    uint32_t nCh = channelCount(fmt);

    // Build ParameterGrid for ndotv lookup (needed by Reference profile)
    ParameterGrid grid(w, h);

    for (uint32_t vi = 0; vi < h; ++vi) {
        for (uint32_t ri = 0; ri < w; ++ri) {
            const LUTCell& cell = lut.at(ri, vi);

            if (!cell.completed) {
                throw std::runtime_error("buildAmplitudeTexture: cell (" +
                    std::to_string(ri) + "," + std::to_string(vi) + ") is not completed");
            }

            size_t pixelIndex = static_cast<size_t>(vi) * w + ri;
            std::byte* dst = tex.pixels.data() + pixelIndex * bpp;

            if (nCh >= 1) encodeChannel(dst, cell.magnitude,          fmt, 0);
            if (nCh >= 2) encodeChannel(dst, cell.fresnelCoefficient,  fmt, 1);

            if (profile == ExportProfile::Reference && nCh >= 4) {
                // Channel 2 = 0 (reserved)
                encodeChannel(dst, 0.0, fmt, 2);
                // Channel 3 = spherical cap coefficient
                LUTParameters params = grid.evaluate({ri, vi});
                double cap = computeSphericalCapCoefficient(params.ndotv);
                encodeChannel(dst, cap, fmt, 3);
            }
        }
    }

    return tex;
}

} // namespace ltc
