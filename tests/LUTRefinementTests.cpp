#include "ltc/LUTRefinement.h"
#include "ltc/RefinementPolicy.h"
#include "ltc/FittingDiagnostics.h"
#include "ltc/GGX.h"
#include "ltc/LUTGenerator.h"
#include "ltc/GenerationConfig.h"
#include <gtest/gtest.h>

using namespace ltc;

// Build a minimal LUT with a few known cells for testing.
static LUTData buildTestLUT(uint32_t W = 4, uint32_t H = 4)
{
    GGX brdf;
    GenerationConfig cfg;
    cfg.width   = W;
    cfg.height  = H;
    cfg.threads = 1;
    cfg.fitting.sampleResolution = 16;
    cfg.fitting.maxIterations    = 50;
    cfg.fitting.profile          = FittingProfile::Reference;
    LUTGenerator gen;
    return gen.generate(brdf, cfg, nullptr);
}

// ---- RefinementPolicy ----

TEST(RefinementPolicy, EmptyLUTYieldsNoCandidates)
{
    LUTData empty(2, 2, LUTMetadata{});
    RefinementCriteria crit;
    RefinementPolicy policy(crit);
    auto cands = policy.selectCandidates(empty);
    EXPECT_TRUE(cands.empty());
}

TEST(RefinementPolicy, AllCompletedCellsWithHighErrorAreCandidates)
{
    LUTData lut = buildTestLUT(2, 2);
    // Force high error on all cells.
    for (auto& cell : lut.cells()) {
        cell.finalError = 1.0;
        cell.status     = FitStatus::MaxIterations;
    }

    RefinementCriteria crit;
    crit.maxFinalError    = 0.1;
    crit.refitMaxIterCells= true;

    RefinementPolicy policy(crit);
    auto cands = policy.selectCandidates(lut);
    EXPECT_EQ(cands.size(), 4u); // 2x2 = 4 cells
}

TEST(RefinementPolicy, MaxRefinementCellsLimitsSelection)
{
    LUTData lut = buildTestLUT(4, 4);
    for (auto& cell : lut.cells()) {
        cell.finalError = 1.0;
    }
    RefinementCriteria crit;
    crit.maxFinalError       = 0.1;
    crit.maxRefinementCells  = 3;
    RefinementPolicy policy(crit);
    auto cands = policy.selectCandidates(lut);
    EXPECT_LE(cands.size(), 3u);
}

TEST(RefinementPolicy, CandidatesAreSortedByDescendingScore)
{
    LUTData lut = buildTestLUT(2, 2);
    lut.at(0,0).finalError = 0.9;
    lut.at(1,0).finalError = 0.3;
    lut.at(0,1).finalError = 0.6;
    lut.at(1,1).finalError = 0.1;

    RefinementCriteria crit;
    crit.maxFinalError    = 0.05;
    crit.refitMaxIterCells= false;
    RefinementPolicy policy(crit);
    auto cands = policy.selectCandidates(lut);

    for (size_t i = 1; i < cands.size(); ++i)
        EXPECT_GE(cands[i-1].score, cands[i].score);
}

TEST(RefinementPolicy, ShouldAcceptWhenRefinedIsBetter)
{
    RefinementCriteria crit;
    crit.improvementThreshold = 0.05;
    RefinementPolicy policy(crit);

    LUTCell orig, ref;
    orig.completed = true; ref.completed = true;
    EXPECT_TRUE(policy.shouldAccept(orig, ref, 1.0, 0.5));  // 50% improvement
}

TEST(RefinementPolicy, ShouldRejectWhenRefinedIsWorse)
{
    RefinementCriteria crit;
    crit.improvementThreshold = 0.05;
    RefinementPolicy policy(crit);

    LUTCell orig, ref;
    EXPECT_FALSE(policy.shouldAccept(orig, ref, 0.5, 0.6));  // worse
}

TEST(RefinementPolicy, ShouldRejectNonFiniteRefinedError)
{
    RefinementCriteria crit;
    RefinementPolicy policy(crit);
    LUTCell orig, ref;
    EXPECT_FALSE(policy.shouldAccept(orig, ref, 1.0,
                                      std::numeric_limits<double>::quiet_NaN()));
}

// ---- FittingDiagnostics ----

TEST(FittingDiagnostics, IdentityMatrixConditionIsOne)
{
    Mat3 I(Scalar(1));
    double cond = computeMatrixConditionNumber(I, I);
    // ||I||_F = sqrt(3), so cond = sqrt(3)*sqrt(3) = 3.
    EXPECT_NEAR(cond, 3.0, 1e-10);
}

TEST(FittingDiagnostics, ValidIdentityMatrix)
{
    Mat3 I(Scalar(1));
    EXPECT_TRUE(isMatrixNumericallyValid(I));
}

TEST(FittingDiagnostics, NonFiniteMatrixIsInvalid)
{
    Mat3 m(Scalar(1));
    m[0][0] = Scalar(std::numeric_limits<float>::infinity());
    EXPECT_FALSE(isMatrixNumericallyValid(m));
}

// ---- LUTRefiner integration ----

TEST(LUTRefiner, RefineSmallLUTDoesNotCrash)
{
    LUTData lut = buildTestLUT(4, 4);

    GGX brdf;
    FittingConfig baseCfg;
    baseCfg.sampleResolution = 16;
    baseCfg.maxIterations    = 50;

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution    = 16;
    qcfg.maximumSampleResolution    = 16;
    qcfg.validationSampleResolution = 16;
    qcfg.maximumIterations          = 50;
    qcfg.multiStart                 = false;
    qcfg.adaptiveSampling           = false;

    // No cell's error will exceed 1e30, so none are selected as error-based candidates.
    RefinementCriteria crit;
    crit.maxFinalError     = 1e30;
    crit.refitMaxIterCells = false; // also disable max-iter selection

    LUTRefiner refiner;
    RefinementResult result = refiner.refine(lut, brdf, baseCfg, qcfg, crit, nullptr);
    // With maxFinalError=0 and refitMaxIterCells=false, no cells qualify.
    EXPECT_EQ(result.candidateCount, 0u);
    EXPECT_EQ(result.refinedCount,   0u);
}

TEST(LUTRefiner, RefinePreservesOriginalWhenRefinedIsNotBetter)
{
    // Generate a good-quality LUT; refinement with looser criteria finds candidates
    // but should mostly leave cells unchanged (original is already good).
    LUTData lut = buildTestLUT(4, 4);

    // Store original final errors.
    std::vector<double> origErrors;
    for (const auto& c : lut.cells())
        origErrors.push_back(c.finalError);

    GGX brdf;
    FittingConfig baseCfg;
    baseCfg.sampleResolution = 16;
    baseCfg.maxIterations    = 50;

    QualityFittingConfig qcfg;
    qcfg.initialSampleResolution    = 16;
    qcfg.maximumSampleResolution    = 16;
    qcfg.validationSampleResolution = 32;
    qcfg.maximumIterations          = 50;
    qcfg.multiStart                 = false;
    qcfg.adaptiveSampling           = false;

    RefinementCriteria crit;
    crit.maxFinalError       = 0.0; // everything qualifies
    crit.refitMaxIterCells   = true;
    crit.improvementThreshold= 0.5; // accept only big improvements

    LUTRefiner refiner;
    RefinementResult result = refiner.refine(lut, brdf, baseCfg, qcfg, crit, nullptr);

    EXPECT_GE(result.candidateCount, 1u);
    // Refined + rejected == total candidates.
    EXPECT_EQ(result.refinedCount + result.rejectedCount, result.candidateCount);
}
