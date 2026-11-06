#include "ltc/Fitter.h"
#include "ltc/QualityFitting.h"
#include "ltc/BRDFIntegration.h"
#include "ltc/GGX.h"
#include "ltc/LUTGenerator.h"
#include "ltc/GenerationConfig.h"
#include <gtest/gtest.h>
#include <chrono>
#include <cmath>

using namespace ltc;

// Performance tests: do not enforce absolute wall-clock limits (machine-dependent).
// Instead verify: determinism, correct result shape, and that multi-thread
// results match single-thread results.

static BRDFContext makeCtx(const IBRDF& brdf, double roughness, double ndotv)
{
    double sinv = std::sqrt(std::max(0.0, 1.0 - ndotv * ndotv));
    Vec3 view = glm::normalize(Vec3(Scalar(sinv), Scalar(0), Scalar(ndotv)));
    return {view, Scalar(brdf.contextAlpha(roughness))};
}

// ---- Timing is finite and positive ----

TEST(PerformanceRegression, SingleFitCompletesInFiniteTime)
{
    GGX brdf;
    BRDFContext ctx = makeCtx(brdf, 0.5, 0.7);
    FittingConfig cfg;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 100;

    auto t0 = std::chrono::steady_clock::now();
    LTCFitter fitter;
    fitter.fit(brdf, ctx, cfg);
    auto t1 = std::chrono::steady_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    EXPECT_GT(ms, 0.0);
    EXPECT_TRUE(std::isfinite(ms));
    // Sanity bound: single-point Reference fit should be well under 10 seconds.
    EXPECT_LT(ms, 10000.0);
}

TEST(PerformanceRegression, QualityFitCompletesInFiniteTime)
{
    GGX brdf;
    BRDFContext ctx = makeCtx(brdf, 0.5, 0.7);

    FittingConfig baseCfg;
    baseCfg.sampleResolution = 16;
    baseCfg.maxIterations    = 50;

    BRDFIntegrator integrator;
    BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, baseCfg);

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution    = 16;
    qcfg.maximumSampleResolution    = 32;
    qcfg.validationSampleResolution = 32;
    qcfg.maximumIterations          = 50;
    qcfg.maximumRestarts            = 2;

    auto t0 = std::chrono::steady_clock::now();
    QualityFitter qFitter;
    qFitter.fit(brdf, ctx, integ, baseCfg, qcfg);
    auto t1 = std::chrono::steady_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    EXPECT_GT(ms, 0.0);
    EXPECT_LT(ms, 60000.0); // well under 1 minute for a small quality fit
}

// ---- Multi-thread vs single-thread consistency ----

TEST(PerformanceRegression, MultiThreadMatchesSingleThread)
{
    GGX brdf;

    GenerationConfig cfg1, cfg2;
    cfg1.width  = cfg2.width  = 4;
    cfg1.height = cfg2.height = 4;
    cfg1.threads = 1;
    cfg2.threads = 2;
    cfg1.fitting.sampleResolution = cfg2.fitting.sampleResolution = 16;
    cfg1.fitting.maxIterations    = cfg2.fitting.maxIterations    = 50;
    cfg1.fitting.profile = cfg2.fitting.profile = FittingProfile::Reference;

    LUTGenerator gen;
    LUTData lut1 = gen.generate(brdf, cfg1, nullptr);
    LUTData lut2 = gen.generate(brdf, cfg2, nullptr);

    ASSERT_EQ(lut1.width(),  lut2.width());
    ASSERT_EQ(lut1.height(), lut2.height());

    for (uint32_t v = 0; v < lut1.height(); ++v) {
        for (uint32_t r = 0; r < lut1.width(); ++r) {
            const LUTCell& c1 = lut1.at(r, v);
            const LUTCell& c2 = lut2.at(r, v);
            EXPECT_EQ(c1.completed, c2.completed);
            if (c1.completed && c2.completed) {
                EXPECT_NEAR(c1.parameters.m11, c2.parameters.m11, 1e-10);
                EXPECT_NEAR(c1.parameters.m22, c2.parameters.m22, 1e-10);
                EXPECT_NEAR(c1.parameters.m13, c2.parameters.m13, 1e-10);
            }
        }
    }
}

// ---- Budget exhaustion is handled gracefully ----

TEST(PerformanceRegression, EvaluationBudgetExhaustion)
{
    GGX brdf;
    BRDFContext ctx = makeCtx(brdf, 0.3, 0.6);

    FittingConfig baseCfg;
    baseCfg.sampleResolution = 16;
    baseCfg.maxIterations    = 10;

    BRDFIntegrator integrator;
    BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, baseCfg);

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution    = 16;
    qcfg.maximumSampleResolution    = 64;
    qcfg.validationSampleResolution = 32;
    qcfg.maximumIterations          = 10;
    qcfg.maximumRestarts            = 10;
    qcfg.maximumTotalEvaluations    = 1; // extremely tight budget

    QualityFitter fitter;
    QualityFittingResult qr = fitter.fit(brdf, ctx, integ, baseCfg, qcfg);

    // Should still return a valid (best-effort) result, not crash.
    bool valid = (qr.base.status != FitStatus::InvalidInput);
    EXPECT_TRUE(valid);
}
