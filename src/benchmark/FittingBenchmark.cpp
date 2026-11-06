#include "ltc/Benchmark.h"
#include "ltc/BRDFIntegration.h"
#include "ltc/BRDFRegistry.h"
#include "ltc/Fitter.h"
#include "ltc/QualityFitting.h"
#include <chrono>
#include <cmath>

namespace ltc {

std::vector<BenchmarkCase> defaultBenchmarkCases(const std::string& brdfName)
{
    return {
        {brdfName, 1.00, 1.00, "broad lobe, normal incidence"},
        {brdfName, 0.70, 0.80, "moderate roughness"},
        {brdfName, 0.50, 0.50, "typical oblique view"},
        {brdfName, 0.20, 0.30, "sharp lobe"},
        {brdfName, 0.05, 0.10, "difficult grazing configuration"},
    };
}

std::vector<BenchmarkResult> runFitBenchmark(
    const std::string& brdfName,
    double             param,
    double             ndotv,
    bool               compareProfiles)
{
    std::vector<BenchmarkResult> results;

    BRDFRegistry registry = BRDFRegistry::createDefault();
    if (!registry.contains(brdfName)) {
        BenchmarkResult bad;
        bad.testCase      = {brdfName, param, ndotv, ""};
        bad.success       = false;
        bad.errorMessage  = "unknown BRDF: " + brdfName;
        results.push_back(bad);
        return results;
    }

    auto brdf = registry.create(brdfName);

    double sinv = std::sqrt(std::max(0.0, 1.0 - ndotv * ndotv));
    Vec3 view = glm::normalize(Vec3(Scalar(sinv), Scalar(0), Scalar(ndotv)));
    BRDFContext ctx{view, Scalar(brdf->contextAlpha(param))};

    auto runProfile = [&](FittingProfile profile) -> BenchmarkResult {
        BenchmarkResult br;
        br.testCase = {brdfName, param, ndotv, ""};
        br.profile  = profile;

        try {
            FittingConfig cfg;
            cfg.profile          = profile;
            cfg.sampleResolution = 32;
            cfg.maxIterations    = (profile == FittingProfile::Quality) ? 300 : 100;
            cfg.tolerance        = (profile == FittingProfile::Quality) ? 1e-7 : 1e-5;

            BRDFIntegrator integrator;

            auto t0 = std::chrono::steady_clock::now();
            BRDFIntegrationResult integ = integrator.integrate(*brdf, ctx, cfg);
            auto t1 = std::chrono::steady_clock::now();

            br.metrics.integrationDurationMs =
                std::chrono::duration<double, std::milli>(t1 - t0).count();

            FittingResult fitResult;
            if (profile == FittingProfile::Quality) {
                QualityFittingConfig qcfg;
                QualityFitter qFitter;
                auto t2 = std::chrono::steady_clock::now();
                QualityFittingResult qr = qFitter.fit(*brdf, ctx, integ, cfg, qcfg);
                auto t3 = std::chrono::steady_clock::now();
                fitResult = qr.base;
                br.metrics.optimizationDurationMs =
                    std::chrono::duration<double, std::milli>(t3 - t2).count();
                br.metrics.restarts              = qr.diagnostics.restartCount;
                br.metrics.validationObjective   = qr.diagnostics.validationObjective;
                br.metrics.objectiveEvaluations  = qr.diagnostics.totalEvaluations;
            } else {
                LTCFitter fitter;
                auto t2 = std::chrono::steady_clock::now();
                fitResult = fitter.fit(*brdf, ctx, integ, cfg);
                auto t3 = std::chrono::steady_clock::now();
                br.metrics.optimizationDurationMs =
                    std::chrono::duration<double, std::milli>(t3 - t2).count();
                br.metrics.objectiveEvaluations  = fitResult.evaluations;
            }

            br.metrics.totalDurationMs  =
                br.metrics.integrationDurationMs + br.metrics.optimizationDurationMs;
            br.metrics.initialObjective = fitResult.initialError;
            br.metrics.finalObjective   = fitResult.finalError;
            br.metrics.iterations       = fitResult.iterations;
            br.metrics.converged        = (fitResult.status == FitStatus::Converged);
            br.metrics.terminationStatus= (fitResult.status == FitStatus::Converged)
                                             ? "converged" : "max_iterations";
            br.success = true;

        } catch (const std::exception& e) {
            br.success       = false;
            br.errorMessage  = e.what();
        }
        return br;
    };

    results.push_back(runProfile(FittingProfile::Reference));
    if (compareProfiles)
        results.push_back(runProfile(FittingProfile::Quality));

    return results;
}

} // namespace ltc
