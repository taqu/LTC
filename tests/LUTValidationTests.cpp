#include <gtest/gtest.h>
#include "ltc/LUTValidation.h"
#include "ltc/LUTGenerator.h"
#include "ltc/GGX.h"

using namespace ltc;

static GenerationConfig valConfig()
{
    GenerationConfig c;
    c.width  = 4;
    c.height = 4;
    c.threads = 1;
    c.fitting.sampleResolution = 8;
    c.fitting.maxIterations    = 20;
    return c;
}

// ---- Basic validation of a generated LUT ----

TEST(LUTValidation, CompleteGeneratedLUTIsValid)
{
    GGX brdf;
    LUTGenerator gen;
    LUTValidator validator;
    auto lut = gen.generate(brdf, valConfig());
    auto r = validator.validate(lut);

    EXPECT_TRUE(r.isComplete);
    EXPECT_EQ(r.totalCells, 16u);
    EXPECT_EQ(r.completedCells, 16u);
    EXPECT_EQ(r.failedCells, 0u);
    EXPECT_TRUE(r.allFinite);
    EXPECT_TRUE(r.allPositiveMagnitude);
    EXPECT_GT(r.finiteErrorCount, 0u);
    EXPECT_GE(r.minFinalError, 0.0);
    EXPECT_GE(r.maxFinalError, r.minFinalError);
    EXPECT_GE(r.meanFinalError, 0.0);
    EXPECT_GE(r.rmsFinalError, 0.0);
}

// ---- Incomplete LUT detected ----

TEST(LUTValidation, IncompleteLUTReported)
{
    LUTMetadata meta;
    meta.width = 2;
    meta.height = 2;
    LUTData lut(2, 2, meta);
    // Leave all cells incomplete (default state)

    LUTValidator validator;
    auto r = validator.validate(lut);

    EXPECT_FALSE(r.isComplete);
    EXPECT_EQ(r.completedCells, 0u);
    EXPECT_EQ(r.totalCells, 4u);
}

// ---- Continuity statistics ----

TEST(LUTValidation, ContinuityJumpsAreNonNegative)
{
    GGX brdf;
    LUTGenerator gen;
    LUTValidator validator;
    auto lut = gen.generate(brdf, valConfig());
    auto r = validator.validate(lut);

    EXPECT_GE(r.maxM11Jump, 0.0);
    EXPECT_GE(r.maxM22Jump, 0.0);
    EXPECT_GE(r.maxM13Jump, 0.0);
    EXPECT_GE(r.maxMagnitudeJump, 0.0);
}

// ---- Error statistics monotonicity ----

TEST(LUTValidation, MeanErrorIsBetweenMinAndMax)
{
    GGX brdf;
    LUTGenerator gen;
    LUTValidator validator;
    auto lut = gen.generate(brdf, valConfig());
    auto r = validator.validate(lut);

    if (r.finiteErrorCount > 0) {
        EXPECT_GE(r.meanFinalError, r.minFinalError - 1e-15);
        EXPECT_LE(r.meanFinalError, r.maxFinalError + 1e-15);
    }
}

// ---- RMS >= mean for non-negative values ----

TEST(LUTValidation, RMSGeqMean)
{
    GGX brdf;
    LUTGenerator gen;
    LUTValidator validator;
    auto lut = gen.generate(brdf, valConfig());
    auto r = validator.validate(lut);

    if (r.finiteErrorCount > 1)
        EXPECT_GE(r.rmsFinalError, r.meanFinalError - 1e-12);
}

// ---- Convergence count consistent ----

TEST(LUTValidation, StatusCountsSumToCompleted)
{
    GGX brdf;
    LUTGenerator gen;
    LUTValidator validator;
    auto lut = gen.generate(brdf, valConfig());
    auto r = validator.validate(lut);

    EXPECT_EQ(r.convergedCells + r.maxIterCells + r.failedCells, r.completedCells);
}

// ---- Larger LUT validation ----

TEST(LUTValidation, Validate8x8)
{
    GGX brdf;
    GenerationConfig cfg = valConfig();
    cfg.width  = 8;
    cfg.height = 8;
    LUTGenerator gen;
    LUTValidator validator;
    auto lut = gen.generate(brdf, cfg);
    auto r = validator.validate(lut);

    EXPECT_TRUE(r.isComplete);
    EXPECT_EQ(r.totalCells, 64u);
    EXPECT_EQ(r.completedCells, 64u);
    EXPECT_TRUE(r.allFinite);
}
