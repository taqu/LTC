#include <gtest/gtest.h>
#include "ltc/LUTGenerator.h"
#include "ltc/GGX.h"
#include <cmath>

using namespace ltc;

static GenerationConfig makeConfig(uint32_t w, uint32_t h, uint32_t threads)
{
    GenerationConfig c;
    c.width   = w;
    c.height  = h;
    c.threads = threads;
    c.fitting.sampleResolution = 8;
    c.fitting.maxIterations    = 20;
    return c;
}

// Helper: compare two LUTs cell-by-cell for identical parameters.
static void compareLUTs(const LUTData& a, const LUTData& b)
{
    ASSERT_EQ(a.width(),  b.width());
    ASSERT_EQ(a.height(), b.height());
    for (uint32_t t = 0; t < a.height(); ++t) {
        for (uint32_t r = 0; r < a.width(); ++r) {
            SCOPED_TRACE("cell (" + std::to_string(r) + "," + std::to_string(t) + ")");
            const auto& ca = a.at(r, t);
            const auto& cb = b.at(r, t);
            EXPECT_EQ(ca.completed, cb.completed);
            EXPECT_EQ(ca.status, cb.status);
            EXPECT_DOUBLE_EQ(ca.parameters.m11, cb.parameters.m11);
            EXPECT_DOUBLE_EQ(ca.parameters.m22, cb.parameters.m22);
            EXPECT_DOUBLE_EQ(ca.parameters.m13, cb.parameters.m13);
            EXPECT_DOUBLE_EQ(ca.magnitude,       cb.magnitude);
            EXPECT_DOUBLE_EQ(ca.finalError,      cb.finalError);
        }
    }
}

// ---- Sequential vs parallel equivalence ----

TEST(ParallelGeneration, TwoThreadsMatchSequential)
{
    GGX brdf;
    LUTGenerator gen;
    auto seq = gen.generate(brdf, makeConfig(4, 4, 1));
    auto par = gen.generate(brdf, makeConfig(4, 4, 2));
    compareLUTs(seq, par);
}

TEST(ParallelGeneration, FourThreadsMatchSequential)
{
    GGX brdf;
    LUTGenerator gen;
    auto seq = gen.generate(brdf, makeConfig(8, 8, 1));
    auto par = gen.generate(brdf, makeConfig(8, 8, 4));
    compareLUTs(seq, par);
}

TEST(ParallelGeneration, MoreThreadsThanRowsMatchSequential)
{
    GGX brdf;
    LUTGenerator gen;
    // 2 roughness rows, 8 threads → more threads than rows
    auto seq = gen.generate(brdf, makeConfig(2, 4, 1));
    auto par = gen.generate(brdf, makeConfig(2, 4, 8));
    compareLUTs(seq, par);
}

TEST(ParallelGeneration, OneThreadSameAsSequentialPath)
{
    GGX brdf;
    LUTGenerator gen;
    auto a = gen.generate(brdf, makeConfig(4, 4, 1));
    auto b = gen.generate(brdf, makeConfig(4, 4, 1));
    compareLUTs(a, b);
}

// ---- All cells complete ----

TEST(ParallelGeneration, AllCellsCompleteParallel)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, makeConfig(8, 8, 4));
    for (uint32_t t = 0; t < lut.height(); ++t)
        for (uint32_t a = 0; a < lut.width(); ++a)
            EXPECT_TRUE(lut.at(a, t).completed);
}

// ---- No missing cells ----

TEST(ParallelGeneration, NoCellsMissedParallel)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, makeConfig(8, 8, 2));
    uint32_t completed = 0;
    for (const auto& cell : lut.cells()) if (cell.completed) ++completed;
    EXPECT_EQ(completed, lut.totalCells());
}

// ---- Determinism across runs ----

TEST(ParallelGeneration, ParallelIsReproducible)
{
    GGX brdf;
    LUTGenerator gen;
    auto run1 = gen.generate(brdf, makeConfig(4, 4, 2));
    auto run2 = gen.generate(brdf, makeConfig(4, 4, 2));
    compareLUTs(run1, run2);
}
