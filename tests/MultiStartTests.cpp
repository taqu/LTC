#include "ltc/InitialGuessStrategy.h"
#include "ltc/QualityFitting.h"
#include "ltc/BRDFIntegration.h"
#include "ltc/GGX.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace ltc;

// ---- InitialGuessStrategy ----

TEST(InitialGuessStrategy, NeighborIsFirst)
{
    InitialGuessStrategy strat;
    LTCInitialGuess neighbor{1.5, 1.2, 0.1};
    auto candidates = strat.generateCandidates(neighbor, nullptr, 10);
    ASSERT_GE(candidates.size(), 1u);
    EXPECT_EQ(candidates[0].type, InitialGuessType::NeighborCell);
    EXPECT_DOUBLE_EQ(candidates[0].guess.m11, neighbor.m11);
    EXPECT_DOUBLE_EQ(candidates[0].guess.m22, neighbor.m22);
    EXPECT_DOUBLE_EQ(candidates[0].guess.m13, neighbor.m13);
}

TEST(InitialGuessStrategy, IdentityIncluded)
{
    InitialGuessStrategy strat;
    LTCInitialGuess neighbor{1.5, 1.2, 0.1};
    auto candidates = strat.generateCandidates(neighbor, nullptr, 10);
    bool hasIdentity = false;
    for (const auto& c : candidates)
        if (c.type == InitialGuessType::Identity) hasIdentity = true;
    EXPECT_TRUE(hasIdentity);
}

TEST(InitialGuessStrategy, RespectsMaxCandidates)
{
    InitialGuessStrategy strat;
    LTCInitialGuess neighbor{1.0, 1.0, 0.0};
    for (uint32_t limit = 1; limit <= 6; ++limit) {
        auto cands = strat.generateCandidates(neighbor, nullptr, limit);
        EXPECT_LE(cands.size(), static_cast<size_t>(limit));
    }
}

TEST(InitialGuessStrategy, PreviousBestIncluded)
{
    InitialGuessStrategy strat;
    LTCInitialGuess neighbor{1.5, 1.2, 0.1};
    LTCInitialGuess prev{0.8, 0.9, -0.05};
    auto candidates = strat.generateCandidates(neighbor, &prev, 10);
    bool hasPrev = false;
    for (const auto& c : candidates)
        if (c.type == InitialGuessType::PreviousBest) hasPrev = true;
    EXPECT_TRUE(hasPrev);
}

TEST(InitialGuessStrategy, NoDuplicateNeighborIdentity)
{
    InitialGuessStrategy strat;
    // When neighbor IS the identity, identity should not be added separately.
    LTCInitialGuess neighbor{1.0, 1.0, 0.0};
    auto cands = strat.generateCandidates(neighbor, nullptr, 10);
    int identityCount = 0;
    for (const auto& c : cands)
        if (c.type == InitialGuessType::Identity) ++identityCount;
    EXPECT_EQ(identityCount, 0);
}

TEST(InitialGuessStrategy, Deterministic)
{
    InitialGuessStrategy strat;
    LTCInitialGuess neighbor{0.7, 0.8, 0.05};
    auto c1 = strat.generateCandidates(neighbor, nullptr, 8);
    auto c2 = strat.generateCandidates(neighbor, nullptr, 8);
    ASSERT_EQ(c1.size(), c2.size());
    for (size_t i = 0; i < c1.size(); ++i) {
        EXPECT_EQ(c1[i].type,     c2[i].type);
        EXPECT_DOUBLE_EQ(c1[i].guess.m11, c2[i].guess.m11);
        EXPECT_DOUBLE_EQ(c1[i].guess.m22, c2[i].guess.m22);
        EXPECT_DOUBLE_EQ(c1[i].guess.m13, c2[i].guess.m13);
    }
}

// ---- QualityFitter basic sanity ----

TEST(QualityFitter, FitsGGXNormalIncidence)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0, 0, 1), Scalar(0.5)};
    BRDFIntegrator integrator;
    FittingConfig baseCfg;
    baseCfg.sampleResolution = 16;
    BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, baseCfg);

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution = 16;
    qcfg.maximumSampleResolution = 32;
    qcfg.validationSampleResolution = 32;
    qcfg.maximumIterations = 100;
    qcfg.maximumRestarts   = 2;

    QualityFitter fitter;
    QualityFittingResult qr = fitter.fit(brdf, ctx, integ, baseCfg, qcfg);
    EXPECT_NE(qr.base.status, FitStatus::InvalidInput);
    EXPECT_NE(qr.base.status, FitStatus::NumericalFailure);
    EXPECT_GT(qr.base.magnitude, 0.0);
    EXPECT_GE(qr.diagnostics.validationObjective, 0.0);
}

TEST(QualityFitter, MultiStartProducesValidResult)
{
    GGX brdf;
    double sinv = std::sqrt(std::max(0.0, 1.0 - 0.7 * 0.7));
    BRDFContext ctx{glm::normalize(Vec3(Scalar(sinv), 0, Scalar(0.7))), Scalar(0.3)};

    BRDFIntegrator integrator;
    FittingConfig baseCfg;
    baseCfg.sampleResolution = 16;
    BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, baseCfg);

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution    = 16;
    qcfg.maximumSampleResolution    = 16; // single stage
    qcfg.validationSampleResolution = 32;
    qcfg.maximumIterations          = 100;
    qcfg.maximumRestarts            = 3;
    qcfg.multiStart                 = true;
    qcfg.adaptiveSampling           = false;

    QualityFitter fitter;
    QualityFittingResult qr = fitter.fit(brdf, ctx, integ, baseCfg, qcfg);
    EXPECT_NE(qr.base.status, FitStatus::InvalidInput);
    EXPECT_GT(qr.base.parameters.m11, 0.0);
    EXPECT_GT(qr.base.parameters.m22, 0.0);
}

TEST(QualityFitter, LogSpaceScalesYieldPositiveScales)
{
    GGX brdf;
    double sinv = std::sqrt(std::max(0.0, 1.0 - 0.5 * 0.5));
    BRDFContext ctx{glm::normalize(Vec3(Scalar(sinv), 0, Scalar(0.5))), Scalar(0.4)};

    BRDFIntegrator integrator;
    FittingConfig baseCfg;
    baseCfg.sampleResolution = 16;
    BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, baseCfg);

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution = 16;
    qcfg.maximumSampleResolution = 16;
    qcfg.validationSampleResolution = 32;
    qcfg.maximumIterations = 100;
    qcfg.logSpaceScales    = true;
    qcfg.multiStart        = false;
    qcfg.adaptiveSampling  = false;

    QualityFitter fitter;
    QualityFittingResult qr = fitter.fit(brdf, ctx, integ, baseCfg, qcfg);
    EXPECT_NE(qr.base.status, FitStatus::NumericalFailure);
    EXPECT_GT(qr.base.parameters.m11, 0.0);
    EXPECT_GT(qr.base.parameters.m22, 0.0);
}
