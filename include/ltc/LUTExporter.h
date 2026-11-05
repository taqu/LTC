#pragma once
#include <filesystem>
#include "LUT.h"
#include "LUTPacking.h"
#include "TextureData.h"

namespace ltc {

struct ExportConfig {
    ExportProfile profile = ExportProfile::Unreal;
    TextureFormat matrixFormat = TextureFormat::RGBA16F;
    TextureFormat amplitudeFormat = TextureFormat::RG16F; // Unreal: RG16F, Reference: RGBA16F
    std::filesystem::path outputDirectory;
    std::string brdfName = "GGX"; // used for filenames
};

struct ExportResult {
    std::filesystem::path matrixFile;
    std::filesystem::path amplitudeFile;
    uint32_t cellCount;
    uint32_t validCells;
    uint32_t maxIterCells; // cells that hit iter limit but have valid result
    // Quantization stats for matrix
    double matMaxAbsError;
    double matMeanAbsError;
    // Quantization stats for amplitude
    double ampMaxAbsError;
    double ampMeanAbsError;
};

class LUTExporter {
public:
    ExportResult exportLUT(const LUTData& lut, const ExportConfig& config) const;
};

} // namespace ltc
