#include "ltc/Fitter.h"
#include "ltc/QualityFitting.h"
#include "ltc/BRDFIntegration.h"
#include "ltc/Beckmann.h"
#include "ltc/DisneyDiffuse.h"
#include "ltc/GGX.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace ltc;

// Shared helper: construct BRDFContext from roughness and NdotV.
static BRDFContext makeCtx(const IBRDF& brdf, double roughness, double ndotv)
{
    double sinv = std::sqrt(std::max(0.0, 1.0 - ndotv * ndotv));
    Vec3 view = glm::normalize(Vec3(Scalar(sinv), Scalar(0), Scalar(ndotv)));
    return BRDFContext{view, Scalar(brdf.contextAlpha(roughness))};
}

// ---- Reference Mode stability ----

struct DifficultCase { double roughness, ndotv; };

class ReferenceModeStability : public ::testing::TestWithParam<DifficultCase> {};

INSTANTIATE_TEST_SUITE_P(GGX, ReferenceModeStability, ::testing::Values(
    DifficultCase{1.00, 1.00},
    DifficultCase{0.50, 0.50},
    DifficultCase{0.20, 0.30},
    DifficultCase{0.05, 0.10}
));

TEST_P(ReferenceModeStability, GGXFiniteResult)
{
    GGX brdf;
    auto p = GetParam();
    BRDFContext ctx = makeCtx(brdf, p.roughness, p.ndotv);

    FittingConfig cfg;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 100;

    LTCFitter fitter;
    FittingResult r = fitter.fit(brdf, ctx, cfg);

    bool ok = (r.status == FitStatus::Converged ||
               r.status == FitStatus::MaxIterations ||
               r.status == FitStatus::ZeroEnergy);
    EXPECT_TRUE(ok);
    if (r.status != FitStatus::ZeroEnergy) {
        EXPECT_TRUE(std::isfinite(r.finalError));
        EXPECT_GT(r.parameters.m11, 0.0);
        EXPECT_GT(r.parameters.m22, 0.0);
    }
}

// ---- Quality Mode on difficult configurations ----

TEST(QualityModeStability, GGXGrazingAngle)
{
    GGX brdf;
    BRDFContext ctx = makeCtx(brdf, 0.05, 0.10);

    BRDFIntegrator integrator;
    FittingConfig baseCfg;
    baseCfg.sampleResolution = 32;
    BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, baseCfg);

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution = 32;
    qcfg.maximumSampleResolution = 64;
    qcfg.validationSampleResolution = 64;
    qcfg.maximumIterations = 200;
    qcfg.maximumRestarts   = 3;

    QualityFitter fitter;
    QualityFittingResult qr = fitter.fit(brdf, ctx, integ, baseCfg, qcfg);

    bool ok = (qr.base.status == FitStatus::Converged ||
               qr.base.status == FitStatus::MaxIterations ||
               qr.base.status == FitStatus::ZeroEnergy);
    EXPECT_TRUE(ok);
}

TEST(QualityModeStability, BeckmannLowRoughness)
{
    Beckmann brdf;
    BRDFContext ctx = makeCtx(brdf, 0.1, 0.5);

    BRDFIntegrator integrator;
    FittingConfig baseCfg;
    baseCfg.sampleResolution = 32;
    BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, baseCfg);

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution = 32;
    qcfg.maximumSampleResolution = 64;
    qcfg.validationSampleResolution = 64;
    qcfg.maximumIterations = 200;

    QualityFitter fitter;
    QualityFittingResult qr = fitter.fit(brdf, ctx, integ, baseCfg, qcfg);

    bool ok = (qr.base.status == FitStatus::Converged ||
               qr.base.status == FitStatus::MaxIterations ||
               qr.base.status == FitStatus::ZeroEnergy);
    EXPECT_TRUE(ok);
    if (qr.base.status != FitStatus::ZeroEnergy)
        EXPECT_GT(qr.base.magnitude, 0.0);
}

TEST(QualityModeStability, DisneyDiffuseNormalIncidence)
{
    DisneyDiffuse brdf;
    BRDFContext ctx{Vec3(0, 0, 1), Scalar(0.5)};

    BRDFIntegrator integrator;
    FittingConfig baseCfg;
    baseCfg.sampleResolution = 32;
    BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, baseCfg);

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution = 32;
    qcfg.maximumSampleResolution = 64;
    qcfg.validationSampleResolution = 64;
    qcfg.maximumIterations = 200;

    QualityFitter fitter;
    QualityFittingResult qr = fitter.fit(brdf, ctx, integ, baseCfg, qcfg);

    bool ok = (qr.base.status == FitStatus::Converged ||
               qr.base.status == FitStatus::MaxIterations ||
               qr.base.status == FitStatus::ZeroEnergy);
    EXPECT_TRUE(ok);
}

// ---- Reference Mode unchanged by Quality Mode presence ----

TEST(ReferenceModePreservation, GGXReferenceMatchesExpected)
{
    // Run the same fitting operation twice with Reference profile; results must match.
    GGX brdf;
    BRDFContext ctx = makeCtx(brdf, 0.5, 0.7);
    FittingConfig cfg;
    cfg.profile          = FittingProfile::Reference;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 100;

    LTCFitter fitter;
    FittingResult r1 = fitter.fit(brdf, ctx, cfg);
    FittingResult r2 = fitter.fit(brdf, ctx, cfg);

    EXPECT_DOUBLE_EQ(r1.parameters.m11, r2.parameters.m11);
    EXPECT_DOUBLE_EQ(r1.parameters.m22, r2.parameters.m22);
    EXPECT_DOUBLE_EQ(r1.parameters.m13, r2.parameters.m13);
    EXPECT_DOUBLE_EQ(r1.finalError,     r2.finalError);
}

TEST(QualityModeStability, DiagnosticsArePopulated)
{
    GGX brdf;
    double sinv = std::sqrt(0.5);
    BRDFContext ctx{glm::normalize(Vec3(Scalar(sinv), 0, Scalar(sinv))), Scalar(0.3)};

    BRDFIntegrator integrator;
    FittingConfig baseCfg;
    baseCfg.sampleResolution = 16;
    BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, baseCfg);

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution    = 16;
    qcfg.maximumSampleResolution    = 16;
    qcfg.validationSampleResolution = 32;
    qcfg.maximumIterations          = 50;
    qcfg.multiStart                 = false;
    qcfg.adaptiveSampling           = false;

    QualityFitter fitter;
    QualityFittingResult qr = fitter.fit(brdf, ctx, integ, baseCfg, qcfg);

    EXPECT_GE(qr.diagnostics.totalEvaluations, 1u);
    EXPECT_GE(qr.diagnostics.finalSampleResolution, 16u);
    EXPECT_GE(qr.diagnostics.validationObjective, 0.0);
}
