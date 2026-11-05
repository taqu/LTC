#include "ltc/LUTExporter.h"
#include "ltc/DDSReader.h"
#include "ltc/DDSWriter.h"
#include "ltc/Float16.h"
#include "ltc/LUTPacking.h"
#include "ltc/Math.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace ltc {

// Helper: decode a pixel value from bytes for quantization error computation
static double decodeChannel(const std::byte* src, TextureFormat fmt, uint32_t channelOffset) {
    if (fmt == TextureFormat::RG16F || fmt == TextureFormat::RGBA16F) {
        uint16_t h = 0;
        std::memcpy(&h, src + channelOffset * 2, sizeof(h));
        return static_cast<double>(DDSReader::fp16ToFloat(h));
    } else {
        float f = 0.0f;
        std::memcpy(&f, src + channelOffset * 4, sizeof(f));
        return static_cast<double>(f);
    }
}

ExportResult LUTExporter::exportLUT(const LUTData& lut, const ExportConfig& config) const {
    // Validate LUT metadata
    if (lut.width() == 0 || lut.height() == 0) {
        throw std::runtime_error("LUTExporter: LUT has zero dimensions");
    }
    if (config.brdfName.empty()) {
        throw std::runtime_error("LUTExporter: brdfName must not be empty");
    }

    uint32_t totalCells = lut.width() * lut.height();
    uint32_t validCells = 0;
    uint32_t maxIterCells = 0;

    // Validate all cells
    for (uint32_t vi = 0; vi < lut.height(); ++vi) {
        for (uint32_t ri = 0; ri < lut.width(); ++ri) {
            const LUTCell& cell = lut.at(ri, vi);
            if (!cell.completed) {
                throw std::runtime_error("LUTExporter: cell (" +
                    std::to_string(ri) + "," + std::to_string(vi) + ") is not completed");
            }
            ++validCells;
            if (cell.status == FitStatus::MaxIterations) {
                ++maxIterCells;
            }
        }
    }

    // Determine output filenames
    std::string matrixFilename;
    std::string amplitudeFilename;
    if (config.profile == ExportProfile::Unreal) {
        matrixFilename    = config.brdfName + "_LTCMat.dds";
        amplitudeFilename = config.brdfName + "_LTCAmp.dds";
    } else if (config.profile == ExportProfile::Generic) {
        matrixFilename    = config.brdfName + "_LTCMat.dds";
        amplitudeFilename = config.brdfName + "_LTCAmp.dds";
    } else {
        matrixFilename    = config.brdfName + "_LTCMat_ref.dds";
        amplitudeFilename = config.brdfName + "_LTCAmp_ref.dds";
    }

    std::filesystem::path matrixPath    = config.outputDirectory / matrixFilename;
    std::filesystem::path amplitudePath = config.outputDirectory / amplitudeFilename;

    // Determine amplitude format: Reference always uses RGBA16F
    TextureFormat ampFmt = config.amplitudeFormat;
    if (config.profile == ExportProfile::Reference) {
        ampFmt = TextureFormat::RGBA16F;
    }

    // Build textures
    TextureData matrixTex    = buildMatrixTexture(lut, config.profile, config.matrixFormat);
    TextureData amplitudeTex = buildAmplitudeTexture(lut, config.profile, ampFmt);

    // Write DDS files
    if (!config.outputDirectory.empty()) {
        std::filesystem::create_directories(config.outputDirectory);
    }

    DDSWriter::write(matrixPath, matrixTex);
    DDSWriter::write(amplitudePath, amplitudeTex);

    // Compute quantization statistics for matrix texture
    double matMaxAbsError  = 0.0;
    double matMeanAbsError = 0.0;
    uint32_t matChannels = channelCount(config.matrixFormat);
    uint32_t matBpp      = bytesPerPixel(config.matrixFormat);

    {
        double errorSum = 0.0;
        uint64_t errorCount = 0;

        for (uint32_t vi = 0; vi < lut.height(); ++vi) {
            for (uint32_t ri = 0; ri < lut.width(); ++ri) {
                const LUTCell& cell = lut.at(ri, vi);
                PackedLTCMatrix packed = packLTCMatrix(cell.matrix);

                double origVals[4] = { packed.r, packed.g, packed.b, packed.a };

                size_t pixelIndex = static_cast<size_t>(vi) * lut.width() + ri;
                const std::byte* src = matrixTex.pixels.data() + pixelIndex * matBpp;

                for (uint32_t c = 0; c < matChannels && c < 4; ++c) {
                    double decoded = decodeChannel(src, config.matrixFormat, c);
                    double err = std::abs(decoded - origVals[c]);
                    if (err > matMaxAbsError) matMaxAbsError = err;
                    errorSum += err;
                    ++errorCount;
                }
            }
        }

        if (errorCount > 0) {
            matMeanAbsError = errorSum / static_cast<double>(errorCount);
        }
    }

    // Compute quantization statistics for amplitude texture
    double ampMaxAbsError  = 0.0;
    double ampMeanAbsError = 0.0;
    uint32_t ampChannels = channelCount(ampFmt);
    uint32_t ampBpp      = bytesPerPixel(ampFmt);

    {
        double errorSum = 0.0;
        uint64_t errorCount = 0;

        for (uint32_t vi = 0; vi < lut.height(); ++vi) {
            for (uint32_t ri = 0; ri < lut.width(); ++ri) {
                const LUTCell& cell = lut.at(ri, vi);

                double origVals[4] = { cell.magnitude, cell.fresnelCoefficient, 0.0, 0.0 };
                // We only compare 2 channels for Unreal, more for Reference
                uint32_t compareChannels = (config.profile == ExportProfile::Unreal) ? 2 : 2; // ch0,ch1

                size_t pixelIndex = static_cast<size_t>(vi) * lut.width() + ri;
                const std::byte* src = amplitudeTex.pixels.data() + pixelIndex * ampBpp;

                for (uint32_t c = 0; c < compareChannels && c < ampChannels; ++c) {
                    double decoded = decodeChannel(src, ampFmt, c);
                    double err = std::abs(decoded - origVals[c]);
                    if (err > ampMaxAbsError) ampMaxAbsError = err;
                    errorSum += err;
                    ++errorCount;
                }
            }
        }

        if (errorCount > 0) {
            ampMeanAbsError = errorSum / static_cast<double>(errorCount);
        }
    }

    ExportResult result;
    result.matrixFile     = matrixPath;
    result.amplitudeFile  = amplitudePath;
    result.cellCount      = totalCells;
    result.validCells     = validCells;
    result.maxIterCells   = maxIterCells;
    result.matMaxAbsError = matMaxAbsError;
    result.matMeanAbsError = matMeanAbsError;
    result.ampMaxAbsError = ampMaxAbsError;
    result.ampMeanAbsError = ampMeanAbsError;

    return result;
}

} // namespace ltc
