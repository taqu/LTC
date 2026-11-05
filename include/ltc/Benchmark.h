#pragma once
#include "FittingConfig.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ltc {

struct BenchmarkCase
{
    std::string brdfName;
    double      roughness;
    double      ndotv;
    std::string description;
};

struct BenchmarkMetrics
{
    double   initialObjective       = 0.0;
    double   finalObjective         = 0.0;
    double   validationObjective    = 0.0;
    uint64_t objectiveEvaluations   = 0;
    uint32_t iterations             = 0;
    uint32_t restarts               = 0;
    bool     converged              = false;
    std::string terminationStatus;

    double   integrationDurationMs  = 0.0;
    double   optimizationDurationMs = 0.0;
    double   totalDurationMs        = 0.0;
};

struct BenchmarkResult
{
    BenchmarkCase    testCase;
    FittingProfile   profile      = FittingProfile::Reference;
    BenchmarkMetrics metrics;
    bool             success      = false;
    std::string      errorMessage;
};

struct GenerationBenchmarkResult
{
    std::string    brdfName;
    uint32_t       width             = 0;
    uint32_t       height            = 0;
    uint32_t       threads           = 1;
    FittingProfile profile           = FittingProfile::Reference;
    double         totalDurationMs   = 0.0;
    double         meanCellDurationMs= 0.0;
    uint32_t       convergedCells    = 0;
    uint32_t       maxIterCells      = 0;
    uint32_t       failedCells       = 0;
    double         meanFinalError    = 0.0;
};

struct ScalingBenchmarkResult
{
    std::string           brdfName;
    uint32_t              width  = 0;
    uint32_t              height = 0;
    std::vector<uint32_t> threadCounts;
    std::vector<double>   durationMs;
    std::vector<double>   speedupRelativeToSingle;
};

struct BenchmarkReport
{
    std::string toolVersion;
    std::string compiler;
    std::string buildConfig;
    std::string timestamp;

    std::vector<BenchmarkResult>           fitResults;
    std::vector<GenerationBenchmarkResult> generationResults;
    ScalingBenchmarkResult                 scalingResult;
};

// Returns a standard set of (roughness, ndotv) cases for the given BRDF.
std::vector<BenchmarkCase> defaultBenchmarkCases(const std::string& brdfName);

// Run a single-point fitting benchmark for one BRDF configuration.
// If compareProfiles is true, runs both Reference and Quality profiles.
std::vector<BenchmarkResult> runFitBenchmark(
    const std::string& brdfName,
    double             param,
    double             ndotv,
    bool               compareProfiles);

// Run a full LUT generation benchmark.
GenerationBenchmarkResult runGenerationBenchmark(
    const std::string& brdfName,
    uint32_t           width,
    uint32_t           height,
    uint32_t           threads,
    FittingProfile     profile);

// Run generation benchmarks for multiple BRDFs with default configurations.
std::vector<GenerationBenchmarkResult> runSuiteBenchmark(
    const std::vector<std::string>& brdfNames,
    uint32_t                        width,
    uint32_t                        height);

// Run scaling benchmark across different thread counts.
ScalingBenchmarkResult runScalingBenchmark(
    const std::string&         brdfName,
    uint32_t                   width,
    uint32_t                   height,
    const std::vector<uint32_t>& threadCounts);

// Serialize a BenchmarkReport to a JSON file.
void writeBenchmarkReport(const BenchmarkReport& report,
                          const std::filesystem::path& outputPath);

} // namespace ltc
