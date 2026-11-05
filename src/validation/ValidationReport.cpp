#include "ltc/ValidationReport.h"
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace ltc {

namespace {

// Convert double to JSON, mapping NaN/Inf → null
nlohmann::json safeDouble(double v)
{
    if (std::isnan(v) || std::isinf(v)) return nullptr;
    return v;
}

nlohmann::json channelComparisonToJson(const ChannelComparisonStats& s)
{
    nlohmann::json j;
    j["minVal"]      = safeDouble(s.minVal);
    j["maxVal"]      = safeDouble(s.maxVal);
    j["minRef"]      = safeDouble(s.minRef);
    j["maxRef"]      = safeDouble(s.maxRef);
    j["mae"]         = safeDouble(s.mae);
    j["maxAE"]       = safeDouble(s.maxAE);
    j["rmse"]        = safeDouble(s.rmse);
    j["mre"]         = safeDouble(s.mre);
    j["maxRE"]       = safeDouble(s.maxRE);
    j["exceedCount"] = s.exceedCount;
    j["nanCount"]    = s.nanCount;
    j["infCount"]    = s.infCount;
    j["worstX"]      = s.worstX;
    j["worstY"]      = s.worstY;
    return j;
}

nlohmann::json textureComparisonToJson(const TextureComparisonResult& r)
{
    nlohmann::json j;
    j["compatible"]   = r.compatible;
    j["width"]        = r.width;
    j["height"]       = r.height;
    j["channelCount"] = r.channelCount;
    if (!r.incompatibilityReason.empty()) {
        j["incompatibilityReason"] = r.incompatibilityReason;
    }

    nlohmann::json channels = nlohmann::json::array();
    for (const auto& ch : r.channels) {
        channels.push_back(channelComparisonToJson(ch));
    }
    j["channels"] = channels;

    nlohmann::json regions = nlohmann::json::array();
    for (const auto& reg : r.regions) {
        nlohmann::json rj;
        rj["name"] = reg.name;
        rj["x0"] = reg.x0; rj["x1"] = reg.x1;
        rj["y0"] = reg.y0; rj["y1"] = reg.y1;
        nlohmann::json rChannels = nlohmann::json::array();
        for (const auto& ch : reg.channels) {
            rChannels.push_back(channelComparisonToJson(ch));
        }
        rj["channels"] = rChannels;
        regions.push_back(rj);
    }
    j["regions"] = regions;
    return j;
}

nlohmann::json distributionMetricsToJson(const DistributionErrorMetrics& m)
{
    nlohmann::json j;
    j["l1Error"]     = safeDouble(m.l1Error);
    j["l2Error"]     = safeDouble(m.l2Error);
    j["cubicError"]  = safeDouble(m.cubicError);
    j["energyError"] = safeDouble(m.energyError);
    j["maxAbsError"] = safeDouble(m.maxAbsError);
    j["sampleCount"] = m.sampleCount;
    nlohmann::json dir = nlohmann::json::array();
    dir.push_back(safeDouble(m.maxErrorDirection.x));
    dir.push_back(safeDouble(m.maxErrorDirection.y));
    dir.push_back(safeDouble(m.maxErrorDirection.z));
    j["maxErrorDirection"] = dir;
    return j;
}

nlohmann::json lightingResultToJson(const LightingScenarioResult& r)
{
    nlohmann::json j;
    j["id"]                   = r.id;
    j["groundTruth"]          = safeDouble(r.groundTruth);
    j["ltcResult"]            = safeDouble(r.ltcResult);
    j["absError"]             = safeDouble(r.absError);
    j["relError"]             = safeDouble(r.relError);
    j["groundTruthConverged"] = r.groundTruthConverged;
    if (!r.notes.empty()) j["notes"] = r.notes;
    return j;
}

} // anonymous namespace

nlohmann::json reportToJson(const FullValidationReport& report)
{
    nlohmann::json j;
    j["schemaVersion"]        = report.schemaVersion;
    j["toolVersion"]          = report.toolVersion;
    j["validationProfile"]    = report.validationProfile;
    j["referenceAvailable"]   = report.referenceAvailable;

    if (report.matrixComparison.has_value()) {
        j["matrixComparison"] = textureComparisonToJson(*report.matrixComparison);
    } else {
        j["matrixComparison"] = nullptr;
    }

    if (report.amplitudeComparison.has_value()) {
        j["amplitudeComparison"] = textureComparisonToJson(*report.amplitudeComparison);
    } else {
        j["amplitudeComparison"] = nullptr;
    }

    nlohmann::json distResults = nlohmann::json::array();
    for (const auto& m : report.distributionResults) {
        distResults.push_back(distributionMetricsToJson(m));
    }
    j["distributionResults"]   = distResults;
    j["worstDistributionL1"]   = safeDouble(report.worstDistributionL1);
    j["meanDistributionL1"]    = safeDouble(report.meanDistributionL1);

    nlohmann::json lightResults = nlohmann::json::array();
    for (const auto& r : report.lightingResults) {
        lightResults.push_back(lightingResultToJson(r));
    }
    j["lightingResults"] = lightResults;

    nlohmann::json summary;
    summary["passed"]  = report.summary.passed;
    summary["failed"]  = report.summary.failed;
    summary["skipped"] = report.summary.skipped;
    j["summary"] = summary;

    return j;
}

void writeJsonReport(const FullValidationReport& report,
                     const std::filesystem::path& path)
{
    nlohmann::json j = reportToJson(report);
    std::ofstream f(path);
    if (!f) {
        throw std::runtime_error("cannot open report file: " + path.string());
    }
    f << j.dump(2) << "\n";
}

void writeCsvReport(const std::vector<DistributionErrorMetrics>& results,
                    const std::filesystem::path& path)
{
    std::ofstream f(path);
    if (!f) {
        throw std::runtime_error("cannot open CSV report file: " + path.string());
    }

    // Header
    f << "index,l1Error,l2Error,cubicError,energyError,maxAbsError,sampleCount\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& m = results[i];
        f << i << ","
          << m.l1Error << ","
          << m.l2Error << ","
          << m.cubicError << ","
          << m.energyError << ","
          << m.maxAbsError << ","
          << m.sampleCount << "\n";
    }
}

} // namespace ltc
