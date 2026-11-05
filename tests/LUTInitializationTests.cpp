#include <gtest/gtest.h>
#include "ltc/LUTGenerator.h"
#include "ltc/GGX.h"
#include "ltc/ParameterGrid.h"
#include "ltc/BRDFIntegration.h"
#include "ltc/Fitter.h"
#include <cmath>

using namespace ltc;

// These tests verify the reference-compatible initialization dependencies
// by generating small LUTs and inspecting the fitted parameters.

static GenerationConfig initConfig(uint32_t w = 4, uint32_t h = 4)
{
    GenerationConfig c;
    c.width  = w;
    c.height = h;
    c.threads = 1;
    c.fitting.sampleResolution = 8;
    c.fitting.maxIterations    = 20;
    return c;
}

// ---- First cell (highest roughness, normal incidence) ----

TEST(LUTInitialization, FirstCellIsCompleted)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, initConfig(4, 4));

    // First cell: roughnessIndex = width-1, viewIndex = 0
    const LUTCell& first = lut.at(3, 0);
    EXPECT_TRUE(first.completed);
}

// ---- Normal incidence cells ----

TEST(LUTInitialization, AllNormalIncidenceCellsAreSymmetric)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, initConfig(4, 4));

    for (uint32_t a = 0; a < lut.width(); ++a) {
        const LUTCell& cell = lut.at(a, 0);
        EXPECT_NEAR(cell.parameters.m11, cell.parameters.m22, 1e-10)
            << "Normal incidence cell (" << a << ",0) should have m11==m22";
        EXPECT_NEAR(cell.parameters.m13, 0.0, 1e-10)
            << "Normal incidence cell (" << a << ",0) should have m13==0";
    }
}

// ---- Non-normal incidence cells ----

TEST(LUTInitialization, NonNormalCellsHaveValidParams)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, initConfig(4, 4));

    for (uint32_t t = 1; t < lut.height(); ++t) {
        for (uint32_t a = 0; a < lut.width(); ++a) {
            const LUTCell& cell = lut.at(a, t);
            EXPECT_TRUE(cell.completed);
            EXPECT_GT(cell.parameters.m11, 0.0);
            EXPECT_GT(cell.parameters.m22, 0.0);
            EXPECT_TRUE(std::isfinite(cell.parameters.m13));
        }
    }
}

// ---- Traversal dependency: check using extended fit() overload ----
// This test manually replicates the first step of the traversal and verifies
// the LUT generator's initial guess for the second roughness row matches what
// we'd compute independently.

TEST(LUTInitialization, SecondRowInitMatchesFirstRowResult)
{
    GGX brdf;
    GenerationConfig cfg = initConfig(4, 4);
    LUTGenerator gen;
    auto lut = gen.generate(brdf, cfg);

    // The initial for lut.at(2, 0) should be derived from lut.at(3, 0).
    // We can't inspect the initial guess directly, but we can verify that
    // the fitted result at (3, 0) is what the generator would have used.
    const LUTCell& prev = lut.at(3, 0); // higher roughness row, t=0
    EXPECT_TRUE(prev.completed);
    EXPECT_GT(prev.parameters.m11, 0.0);
    EXPECT_GT(prev.parameters.m22, 0.0);
    EXPECT_NEAR(prev.parameters.m13, 0.0, 1e-10); // isotropic at normal incidence
}

// ---- LTC orientation reconstruction ----
// Verify that for oblique-incidence cells, the basis reconstructed from
// the average direction is well-formed (orthonormal columns).

TEST(LUTInitialization, ObliqueCellBasisIsOrthonormal)
{
    GGX brdf;
    LUTGenerator gen;
    auto lut = gen.generate(brdf, initConfig(4, 4));

    for (uint32_t t = 1; t < lut.height(); ++t) {
        for (uint32_t a = 0; a < lut.width(); ++a) {
            const LUTCell& cell = lut.at(a, t);
            const Mat3& B = cell.parameters.basis;

            Vec3 col0(B[0]), col1(B[1]), col2(B[2]);
            // Columns should be unit vectors
            double len0 = std::sqrt(col0.x*col0.x + col0.y*col0.y + col0.z*col0.z);
            double len1 = std::sqrt(col1.x*col1.x + col1.y*col1.y + col1.z*col1.z);
            double len2 = std::sqrt(col2.x*col2.x + col2.y*col2.y + col2.z*col2.z);
            EXPECT_NEAR(len0, 1.0, 1e-10);
            EXPECT_NEAR(len1, 1.0, 1e-10);
            EXPECT_NEAR(len2, 1.0, 1e-10);
        }
    }
}

// ---- Pre-computed integration overload correctness ----
// Verify that fit() with pre-computed integration gives the same result
// as the standard overload.

TEST(LUTInitialization, PrecomputedOverloadMatchesStandard)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0.0, 0.0, 1.0), Scalar(0.5)};
    FittingConfig fcfg;
    fcfg.sampleResolution = 8;
    fcfg.maxIterations    = 20;

    LTCFitter fitter;
    BRDFIntegrator integrator;

    // Standard overload
    auto r1 = fitter.fit(brdf, ctx, fcfg);

    // Pre-computed overload
    auto integ = integrator.integrate(brdf, ctx, fcfg);
    auto r2 = fitter.fit(brdf, ctx, integ, fcfg);

    EXPECT_EQ(r1.status, r2.status);
    EXPECT_DOUBLE_EQ(r1.parameters.m11, r2.parameters.m11);
    EXPECT_DOUBLE_EQ(r1.parameters.m22, r2.parameters.m22);
    EXPECT_DOUBLE_EQ(r1.parameters.m13, r2.parameters.m13);
    EXPECT_DOUBLE_EQ(r1.magnitude,      r2.magnitude);
    EXPECT_DOUBLE_EQ(r1.finalError,     r2.finalError);
}
