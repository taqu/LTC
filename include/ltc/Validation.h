#pragma once
#include "ValidationConfig.h"
#include "ValidationReport.h"
#include "LUT.h"
#include <filesystem>
#include <optional>

namespace ltc {

class ValidationRunner {
public:
    // Run reference DDS comparison
    TextureComparisonResult runReferenceComparison(
        const std::filesystem::path& generatedDir,
        const std::filesystem::path& referenceDir,
        const std::string& brdfName = "GGX") const;

    // Run distribution validation on a LUT
    // Validates a subset of cells (at most maxCells) for performance
    std::vector<DistributionErrorMetrics> runDistributionValidation(
        const LUTData& lut,
        const ValidationConfig& config,
        uint32_t maxCells = 256) const;

    // Run lighting validation using built-in scenarios
    std::vector<LightingScenarioResult> runLightingValidation(
        const LUTData& lut,
        const ValidationConfig& config) const;

    // Full validation pipeline
    FullValidationReport runAll(
        const LUTData& lut,
        const std::filesystem::path& ddsDirectory,
        const ValidationConfig& config,
        const std::string& brdfName = "GGX",
        const std::optional<std::filesystem::path>& referenceDir = std::nullopt) const;
};

} // namespace ltc
