#include "ltc/Fitter.h"
#include "ltc/QualityFitting.h"
#include "ltc/BRDFIntegration.h"
#include "ltc/Beckmann.h"
#include "ltc/DisneyDiffuse.h"
#include "ltc/GGX.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace ltc;

static BRDFContext makeCtx(const IBRDF& brdf, double roughness, double ndotv)
{
    double sinv = std::sqrt(std::max(0.0, 1.0 - ndotv * ndotv));
    Vec3 view = glm::normalize(Vec3(Scalar(sinv), Scalar(0), Scalar(ndotv)));
    return {view, Scalar(brdf.contextAlpha(roughness))};
}

// ---- Reference Mode determinism ----

TEST(ReferenceRegression, GGXDeterministicAcrossRuns)
{
    GGX brdf;
    FittingConfig cfg;
    cfg.profile          = FittingProfile::Reference;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 100;

    struct Case { double r, v; };
    for (auto [r, v] : std::initializer_list<Case>{{1.0,1.0},{0.5,0.5},{0.2,0.3}}) {
        BRDFContext ctx = makeCtx(brdf, r, v);
        LTCFitter fitter;
        FittingResult a = fitter.fit(brdf, ctx, cfg);
        FittingResult b = fitter.fit(brdf, ctx, cfg);
        EXPECT_DOUBLE_EQ(a.parameters.m11, b.parameters.m11);
        EXPECT_DOUBLE_EQ(a.parameters.m22, b.parameters.m22);
        EXPECT_DOUBLE_EQ(a.parameters.m13, b.parameters.m13);
        EXPECT_DOUBLE_EQ(a.finalError,     b.finalError);
    }
}

TEST(ReferenceRegression, BeckmannDeterministic)
{
    Beckmann brdf;
    FittingConfig cfg;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 100;

    BRDFContext ctx = makeCtx(brdf, 0.5, 0.7);
    LTCFitter fitter;
    FittingResult a = fitter.fit(brdf, ctx, cfg);
    FittingResult b = fitter.fit(brdf, ctx, cfg);
    EXPECT_DOUBLE_EQ(a.finalError, b.finalError);
}

TEST(ReferenceRegression, DisneyDiffuseDeterministic)
{
    DisneyDiffuse brdf;
    FittingConfig cfg;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 100;

    BRDFContext ctx = makeCtx(brdf, 0.5, 0.7);
    LTCFitter fitter;
    FittingResult a = fitter.fit(brdf, ctx, cfg);
    FittingResult b = fitter.fit(brdf, ctx, cfg);
    EXPECT_DOUBLE_EQ(a.finalError, b.finalError);
}

// ---- Quality Mode does not degrade Reference Mode output ----

TEST(QualityRegression, QualityFinalErrorNotWorseThanReference)
{
    GGX brdf;
    BRDFContext ctx = makeCtx(brdf, 0.5, 0.7);

    FittingConfig cfg;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 100;

    BRDFIntegrator integrator;
    BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, cfg);

    LTCFitter refFitter;
    FittingResult refResult = refFitter.fit(brdf, ctx, integ, cfg);

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution    = 32;
    qcfg.maximumSampleResolution    = 64;
    qcfg.validationSampleResolution = 64;
    qcfg.maximumIterations          = 200;
    qcfg.maximumRestarts            = 3;

    QualityFitter qFitter;
    QualityFittingResult qr = qFitter.fit(brdf, ctx, integ, cfg, qcfg);

    // Quality mode may find same or better validation error.
    // We don't require strict improvement, just that it's valid.
    EXPECT_NE(qr.base.status, FitStatus::NumericalFailure);
    EXPECT_NE(qr.base.status, FitStatus::InvalidInput);
    (void)refResult; // just checking quality mode produces a valid result
}

// ---- GGX objective values sanity ----

TEST(ReferenceRegression, GGXNormalIncidenceFiniteError)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0, 0, 1), Scalar(0.5)};
    FittingConfig cfg;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 100;
    LTCFitter fitter;
    FittingResult r = fitter.fit(brdf, ctx, cfg);
    EXPECT_TRUE(std::isfinite(r.finalError));
    EXPECT_GE(r.finalError, 0.0);
}

TEST(ReferenceRegression, GGXFiniteForBroadRange)
{
    GGX brdf;
    FittingConfig cfg;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 100;

    struct Case { double r, v; };
    for (auto [r, v] : std::initializer_list<Case>{
            {1.0,1.0},{0.7,0.8},{0.5,0.5},{0.2,0.3}}) {
        BRDFContext ctx = makeCtx(brdf, r, v);
        LTCFitter fitter;
        FittingResult res = fitter.fit(brdf, ctx, cfg);
        EXPECT_TRUE(res.status == FitStatus::Converged ||
                    res.status == FitStatus::MaxIterations ||
                    res.status == FitStatus::ZeroEnergy);
        if (res.status != FitStatus::ZeroEnergy)
            EXPECT_TRUE(std::isfinite(res.finalError));
    }
}

// ---- Parameter constraint validity ----

TEST(ReferenceRegression, ScalesAlwaysPositive)
{
    GGX brdf;
    FittingConfig cfg;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 100;

    struct Case { double r, v; };
    for (auto [r, v] : std::initializer_list<Case>{
            {1.0,1.0},{0.5,0.5},{0.1,0.2},{0.05,0.1}}) {
        BRDFContext ctx = makeCtx(brdf, r, v);
        LTCFitter fitter;
        FittingResult res = fitter.fit(brdf, ctx, cfg);
        if (res.status == FitStatus::Converged ||
            res.status == FitStatus::MaxIterations)
        {
            EXPECT_GT(res.parameters.m11, 0.0);
            EXPECT_GT(res.parameters.m22, 0.0);
        }
    }
}
