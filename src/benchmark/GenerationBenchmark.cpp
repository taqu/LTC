#include "ltc/Benchmark.h"
#include "ltc/BRDFRegistry.h"
#include "ltc/GenerationConfig.h"
#include "ltc/LUTGenerator.h"
#include "ltc/LUTValidation.h"
#include <chrono>

namespace ltc {

GenerationBenchmarkResult runGenerationBenchmark(
    const std::string& brdfName,
    uint32_t           width,
    uint32_t           height,
    uint32_t           threads,
    FittingProfile     profile)
{
    GenerationBenchmarkResult res;
    res.brdfName = brdfName;
    res.width    = width;
    res.height   = height;
    res.threads  = threads;
    res.profile  = profile;

    BRDFRegistry registry = BRDFRegistry::createDefault();
    if (!registry.contains(brdfName)) return res;

    auto brdf = registry.create(brdfName);

    GenerationConfig cfg;
    cfg.width   = width;
    cfg.height  = height;
    cfg.threads = threads;
    cfg.fitting.profile          = profile;
    cfg.fitting.sampleResolution = 32;
    cfg.fitting.maxIterations    = (profile == FittingProfile::Quality) ? 300 : 100;
    cfg.fitting.tolerance        = (profile == FittingProfile::Quality) ? 1e-7 : 1e-5;

    LUTGenerator generator;

    auto t0 = std::chrono::steady_clock::now();
    LUTData lut = generator.generate(*brdf, cfg, nullptr);
    auto t1 = std::chrono::steady_clock::now();

    res.totalDurationMs   = std::chrono::duration<double, std::milli>(t1 - t0).count();
    res.meanCellDurationMs= res.totalDurationMs / (width * height);

    LUTValidator validator;
    ValidationReport report = validator.validate(lut);
    res.convergedCells = report.convergedCells;
    res.maxIterCells   = report.maxIterCells;
    res.failedCells    = report.failedCells;
    res.meanFinalError = report.meanFinalError;

    return res;
}

std::vector<GenerationBenchmarkResult> runSuiteBenchmark(
    const std::vector<std::string>& brdfNames,
    uint32_t                        width,
    uint32_t                        height)
{
    std::vector<GenerationBenchmarkResult> results;
    results.reserve(brdfNames.size() * 2);

    for (const auto& name : brdfNames) {
        results.push_back(runGenerationBenchmark(name, width, height, 1,
                                                  FittingProfile::Reference));
        results.push_back(runGenerationBenchmark(name, width, height, 1,
                                                  FittingProfile::Quality));
    }
    return results;
}

ScalingBenchmarkResult runScalingBenchmark(
    const std::string&          brdfName,
    uint32_t                    width,
    uint32_t                    height,
    const std::vector<uint32_t>& threadCounts)
{
    ScalingBenchmarkResult res;
    res.brdfName     = brdfName;
    res.width        = width;
    res.height       = height;
    res.threadCounts = threadCounts;
    res.durationMs.resize(threadCounts.size(), 0.0);
    res.speedupRelativeToSingle.resize(threadCounts.size(), 1.0);

    double singleThreadMs = 0.0;

    for (size_t i = 0; i < threadCounts.size(); ++i) {
        auto r = runGenerationBenchmark(brdfName, width, height,
                                         threadCounts[i], FittingProfile::Reference);
        res.durationMs[i] = r.totalDurationMs;
        if (threadCounts[i] == 1 || i == 0)
            singleThreadMs = r.totalDurationMs;
    }

    for (size_t i = 0; i < threadCounts.size(); ++i) {
        if (res.durationMs[i] > 0.0 && singleThreadMs > 0.0)
            res.speedupRelativeToSingle[i] = singleThreadMs / res.durationMs[i];
    }

    return res;
}

} // namespace ltc
