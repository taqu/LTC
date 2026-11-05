#include <gtest/gtest.h>
#include "ltc/LUTGenerator.h"
#include "ltc/GGX.h"
#include "ltc/LUTValidation.h"
#include <cmath>

using namespace ltc;

static GenerationConfig smallConfig(uint32_t w = 4, uint32_t h = 4)
{
    GenerationConfig c;
    c.width  = w;
    c.height = h;
    c.threads = 1;
    c.fitting.sampleResolution = 8;
    c.fitting.maxIterations    = 20;
    return c;
}

// ---- Basic generation ----

TEST(LUTGenerator, GeneratesCorrectCellCount)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, smallConfig(4, 4));

    EXPECT_EQ(lut.width(),      4u);
    EXPECT_EQ(lut.height(),     4u);
    EXPECT_EQ(lut.totalCells(), 16u);
}

TEST(LUTGenerator, AllCellsCompleted)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, smallConfig());

    for (uint32_t t = 0; t < lut.height(); ++t)
        for (uint32_t a = 0; a < lut.width(); ++a)
            EXPECT_TRUE(lut.at(a, t).completed) << "cell (" << a << "," << t << ")";
}

TEST(LUTGenerator, AllCellsHaveFiniteCoefficients)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, smallConfig());

    for (uint32_t t = 0; t < lut.height(); ++t) {
        for (uint32_t a = 0; a < lut.width(); ++a) {
            const LUTCell& cell = lut.at(a, t);
            EXPECT_TRUE(std::isfinite(cell.parameters.m11));
            EXPECT_TRUE(std::isfinite(cell.parameters.m22));
            EXPECT_TRUE(std::isfinite(cell.parameters.m13));
            EXPECT_TRUE(std::isfinite(cell.magnitude));
        }
    }
}

TEST(LUTGenerator, AllCellsHavePositiveScaleParams)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, smallConfig());

    for (uint32_t t = 0; t < lut.height(); ++t)
        for (uint32_t a = 0; a < lut.width(); ++a) {
            EXPECT_GT(lut.at(a, t).parameters.m11, 0.0);
            EXPECT_GT(lut.at(a, t).parameters.m22, 0.0);
        }
}

// ---- Determinism ----

TEST(LUTGenerator, Deterministic)
{
    GGX brdf;
    GenerationConfig cfg = smallConfig(4, 4);
    LUTGenerator gen;

    auto lut1 = gen.generate(brdf, cfg);
    auto lut2 = gen.generate(brdf, cfg);

    for (uint32_t t = 0; t < lut1.height(); ++t) {
        for (uint32_t a = 0; a < lut1.width(); ++a) {
            const auto& c1 = lut1.at(a, t);
            const auto& c2 = lut2.at(a, t);
            EXPECT_DOUBLE_EQ(c1.parameters.m11, c2.parameters.m11);
            EXPECT_DOUBLE_EQ(c1.parameters.m22, c2.parameters.m22);
            EXPECT_DOUBLE_EQ(c1.parameters.m13, c2.parameters.m13);
            EXPECT_DOUBLE_EQ(c1.magnitude,       c2.magnitude);
        }
    }
}

// ---- Normal incidence symmetry ----

TEST(LUTGenerator, NormalIncidenceCellsAreSymmetric)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, smallConfig(4, 4));

    for (uint32_t a = 0; a < lut.width(); ++a) {
        const auto& cell = lut.at(a, 0); // t=0 is normal incidence
        // 1D optimization: m11 == m22, m13 == 0
        EXPECT_NEAR(cell.parameters.m11, cell.parameters.m22, 1e-10);
        EXPECT_NEAR(cell.parameters.m13, 0.0, 1e-10);
    }
}

// ---- Validation ----

TEST(LUTGenerator, ValidationPassesOnSmallLUT)
{
    GGX brdf;
    LUTGenerator gen;
    LUTValidator validator;
    auto lut = gen.generate(brdf, smallConfig());
    auto report = validator.validate(lut);

    EXPECT_TRUE(report.isComplete);
    EXPECT_EQ(report.completedCells, lut.totalCells());
    EXPECT_EQ(report.failedCells, 0u);
    EXPECT_TRUE(report.allFinite);
    EXPECT_TRUE(report.allPositiveMagnitude);
}

// ---- Invalid config ----

TEST(LUTGenerator, InvalidConfigThrows)
{
    GGX brdf;
    LUTGenerator gen;

    GenerationConfig bad;
    bad.width = 1; // invalid
    EXPECT_THROW(gen.generate(brdf, bad), std::invalid_argument);
}

// ---- Index formula ----

TEST(LUTGenerator, CellIndexing)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, smallConfig(4, 4));

    // Verify at() uses index = a + t * width
    for (uint32_t t = 0; t < 4; ++t) {
        for (uint32_t a = 0; a < 4; ++a) {
            size_t idx = static_cast<size_t>(a) + static_cast<size_t>(t) * 4;
            const LUTCell& via_at    = lut.at(a, t);
            const LUTCell& via_cells = lut.cells()[idx];
            EXPECT_EQ(&via_at, &via_cells);
        }
    }
}

// ---- Larger 8x8 sanity ----

TEST(LUTGenerator, Generates8x8WithoutCrash)
{
    GGX brdf;
    GenerationConfig cfg = smallConfig(8, 8);
    LUTGenerator gen;
    auto lut = gen.generate(brdf, cfg);
    EXPECT_EQ(lut.totalCells(), 64u);
}
