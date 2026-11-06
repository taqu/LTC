#include "ltc/Benchmark.h"
#include "ltc/Version.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

namespace ltc {

namespace {

std::string currentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string profileName(FittingProfile p)
{
    return (p == FittingProfile::Quality) ? "quality" : "reference";
}

nlohmann::json metricsToJson(const BenchmarkMetrics& m)
{
    nlohmann::json j;
    j["initialObjective"]      = m.initialObjective;
    j["finalObjective"]        = m.finalObjective;
    j["validationObjective"]   = m.validationObjective;
    j["objectiveEvaluations"]  = m.objectiveEvaluations;
    j["iterations"]            = m.iterations;
    j["restarts"]              = m.restarts;
    j["converged"]             = m.converged;
    j["terminationStatus"]     = m.terminationStatus;
    j["integrationDurationMs"] = m.integrationDurationMs;
    j["optimizationDurationMs"]= m.optimizationDurationMs;
    j["totalDurationMs"]       = m.totalDurationMs;
    return j;
}

} // anonymous namespace

void writeBenchmarkReport(const BenchmarkReport& report,
                          const std::filesystem::path& outputPath)
{
    nlohmann::json j;
    j["toolVersion"] = report.toolVersion.empty()
                           ? std::string(Version::string())
                           : report.toolVersion;
    j["compiler"]    = report.compiler;
    j["buildConfig"] = report.buildConfig;
    j["timestamp"]   = report.timestamp.empty() ? currentTimestamp() : report.timestamp;

    // Fit results.
    nlohmann::json jfit = nlohmann::json::array();
    for (const auto& r : report.fitResults) {
        nlohmann::json jr;
        jr["brdf"]         = r.testCase.brdfName;
        jr["roughness"]    = r.testCase.roughness;
        jr["ndotv"]        = r.testCase.ndotv;
        jr["description"]  = r.testCase.description;
        jr["profile"]      = profileName(r.profile);
        jr["success"]      = r.success;
        if (!r.errorMessage.empty()) jr["error"] = r.errorMessage;
        if (r.success) jr["metrics"] = metricsToJson(r.metrics);
        jfit.push_back(jr);
    }
    j["fitResults"] = jfit;

    // Generation results.
    nlohmann::json jgen = nlohmann::json::array();
    for (const auto& r : report.generationResults) {
        nlohmann::json jr;
        jr["brdf"]             = r.brdfName;
        jr["width"]            = r.width;
        jr["height"]           = r.height;
        jr["threads"]          = r.threads;
        jr["profile"]          = profileName(r.profile);
        jr["totalDurationMs"]  = r.totalDurationMs;
        jr["meanCellDurationMs"]= r.meanCellDurationMs;
        jr["convergedCells"]   = r.convergedCells;
        jr["maxIterCells"]     = r.maxIterCells;
        jr["failedCells"]      = r.failedCells;
        jr["meanFinalError"]   = r.meanFinalError;
        jgen.push_back(jr);
    }
    j["generationResults"] = jgen;

    // Scaling result.
    {
        const auto& sc = report.scalingResult;
        nlohmann::json jsc;
        jsc["brdf"]   = sc.brdfName;
        jsc["width"]  = sc.width;
        jsc["height"] = sc.height;
        nlohmann::json jthreads   = nlohmann::json::array();
        nlohmann::json jdurations = nlohmann::json::array();
        nlohmann::json jspeedups  = nlohmann::json::array();
        for (size_t i = 0; i < sc.threadCounts.size(); ++i) {
            jthreads.push_back(sc.threadCounts[i]);
            jdurations.push_back(i < sc.durationMs.size() ? sc.durationMs[i] : 0.0);
            jspeedups.push_back(i < sc.speedupRelativeToSingle.size()
                                    ? sc.speedupRelativeToSingle[i] : 0.0);
        }
        jsc["threadCounts"]             = jthreads;
        jsc["durationMs"]               = jdurations;
        jsc["speedupRelativeToSingle"]  = jspeedups;
        j["scalingResult"] = jsc;
    }

    std::ofstream out(outputPath);
    if (!out.is_open())
        throw std::runtime_error("cannot open benchmark report: " + outputPath.string());
    out << j.dump(2) << "\n";
}

} // namespace ltc
