#pragma once
#include "DistributionValidation.h"
#include "ReferenceComparison.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ltc {

struct LightingScenarioResult {
    std::string id;
    double groundTruth = 0;    // monochrome for simplicity
    double ltcResult = 0;
    double absError = 0;
    double relError = 0;
    bool groundTruthConverged = false;
    std::string notes;
};

struct ValidationSummary {
    int passed = 0;
    int failed = 0;
    int skipped = 0;
};

struct FullValidationReport {
    int schemaVersion = 1;
    std::string toolVersion;
    std::string validationProfile;

    // Reference comparison (optional)
    bool referenceAvailable = false;
    std::optional<TextureComparisonResult> matrixComparison;
    std::optional<TextureComparisonResult> amplitudeComparison;

    // Distribution validation
    std::vector<DistributionErrorMetrics> distributionResults;
    double worstDistributionL1 = 0;
    double meanDistributionL1 = 0;

    // Lighting scenarios
    std::vector<LightingScenarioResult> lightingResults;

    ValidationSummary summary;
};

// Serialize to JSON (NaN/Inf → null)
nlohmann::json reportToJson(const FullValidationReport& report);

// Write JSON report to file
void writeJsonReport(const FullValidationReport& report, const std::filesystem::path& path);

// Write CSV per-distribution results
void writeCsvReport(const std::vector<DistributionErrorMetrics>& results,
                    const std::filesystem::path& path);

} // namespace ltc
