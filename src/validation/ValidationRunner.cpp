#include "ltc/Validation.h"
#include "ltc/ReferenceComparison.h"
#include "ltc/DistributionValidation.h"
#include "ltc/GGX.h"
#include "ltc/ParameterGrid.h"
#include "ltc/Version.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

namespace ltc {

TextureComparisonResult ValidationRunner::runReferenceComparison(
    const std::filesystem::path& generatedDir,
    const std::filesystem::path& referenceDir,
    const std::string& brdfName) const
{
    // Look for matrix and amplitude DDS files in both directories
    // Naming convention: <brdfName>_LTC_Matrix.dds and <brdfName>_LTC_Amplitude.dds
    // Try multiple naming patterns
    auto findFile = [](const std::filesystem::path& dir,
                       const std::string& name) -> std::filesystem::path
    {
        std::filesystem::path p = dir / name;
        if (std::filesystem::exists(p)) return p;
        return {};
    };

    std::string matName   = brdfName + "_LTC_Matrix.dds";
    std::string ampName   = brdfName + "_LTC_Amplitude.dds";

    std::filesystem::path genMat = findFile(generatedDir, matName);
    std::filesystem::path refMat = findFile(referenceDir, matName);

    if (genMat.empty() || refMat.empty()) {
        TextureComparisonResult r;
        r.compatible = false;
        r.incompatibilityReason = "matrix DDS files not found";
        return r;
    }

    return compareTextures(genMat, refMat);
}

std::vector<DistributionErrorMetrics> ValidationRunner::runDistributionValidation(
    const LUTData& lut,
    const ValidationConfig& config,
    uint32_t maxCells) const
{
    std::vector<DistributionErrorMetrics> results;

    if (lut.width() == 0 || lut.height() == 0) return results;

    GGX brdf;
    ParameterGrid grid(lut.width(), lut.height());

    DistributionValidationConfig dvCfg;
    dvCfg.sampleCount = config.distributionSamples;

    // Pick a subset of cells uniformly
    uint32_t totalCells = lut.width() * lut.height();
    uint32_t step = std::max(1u, totalCells / maxCells);

    for (uint32_t idx = 0; idx < totalCells; idx += step) {
        uint32_t ri = idx % lut.width();
        uint32_t vi = idx / lut.width();

        const LUTCell& cell = lut.at(ri, vi);
        if (!cell.completed) continue;

        LUTParameters params = grid.evaluate({ri, vi});

        DistributionErrorMetrics metrics = validateDistribution(brdf, cell, params, dvCfg);
        results.push_back(metrics);
    }

    return results;
}

FullValidationReport ValidationRunner::runAll(
    const LUTData& lut,
    const std::filesystem::path& ddsDirectory,
    const ValidationConfig& config,
    const std::string& brdfName,
    const std::optional<std::filesystem::path>& referenceDir) const
{
    FullValidationReport report;
    report.toolVersion = std::string(Version::string());

    switch (config.profile) {
        case ValidationProfile::Reference: report.validationProfile = "Reference"; break;
        case ValidationProfile::Unreal:    report.validationProfile = "Unreal"; break;
        case ValidationProfile::Quality:   report.validationProfile = "Quality"; break;
    }

    // Reference comparison
    if (referenceDir.has_value()) {
        report.referenceAvailable = true;
        TextureComparisonResult matComp = runReferenceComparison(
            ddsDirectory, *referenceDir, brdfName);
        report.matrixComparison = matComp;

        // Try amplitude comparison too
        std::string ampName = brdfName + "_LTC_Amplitude.dds";
        std::filesystem::path genAmp = ddsDirectory / ampName;
        std::filesystem::path refAmp = *referenceDir / ampName;
        if (std::filesystem::exists(genAmp) && std::filesystem::exists(refAmp)) {
            report.amplitudeComparison = compareTextures(genAmp, refAmp);
        }

        // Track comparison pass/fail
        if (matComp.compatible) {
            bool pass = true;
            for (const auto& ch : matComp.channels) {
                if (ch.rmse > config.thresholds.coefficientRmse) {
                    pass = false;
                    break;
                }
            }
            if (pass) ++report.summary.passed;
            else ++report.summary.failed;
        } else {
            ++report.summary.skipped;
        }
    } else {
        report.referenceAvailable = false;
    }

    // Distribution validation
    report.distributionResults = runDistributionValidation(lut, config);

    if (!report.distributionResults.empty()) {
        double sumL1 = 0;
        double worstL1 = 0;
        for (const auto& m : report.distributionResults) {
            sumL1 += m.l1Error;
            worstL1 = std::max(worstL1, m.l1Error);
        }
        report.worstDistributionL1 = worstL1;
        report.meanDistributionL1 = sumL1 / report.distributionResults.size();

        bool distPass = (worstL1 <= config.thresholds.distributionL1);
        if (distPass) ++report.summary.passed;
        else ++report.summary.failed;
    }

    // Lighting validation
    report.lightingResults = runLightingValidation(lut, config);

    int lightPassed = 0, lightFailed = 0;
    for (const auto& r : report.lightingResults) {
        if (r.relError <= config.thresholds.lightingRelative ||
            r.absError <= config.thresholds.lightingAbsolute) {
            ++lightPassed;
        } else {
            ++lightFailed;
        }
    }
    report.summary.passed += lightPassed;
    report.summary.failed += lightFailed;

    return report;
}

} // namespace ltc
