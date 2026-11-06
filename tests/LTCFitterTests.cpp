#include <gtest/gtest.h>
#include "ltc/Fitter.h"
#include "ltc/GGX.h"
#include "ltc/LTC.h"
#include "ltc/Math.h"
#include <cmath>

using namespace ltc;

static FittingConfig fastConfig()
{
    FittingConfig c;
    c.sampleResolution = 16;
    c.maxIterations    = 50;
    return c;
}

// ---- Basic result sanity ----

TEST(LTCFitter, ResultIsFinite)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0.0, 0.0, 1.0), Scalar(0.5)};

    LTCFitter fitter;
    auto r = fitter.fit(brdf, ctx, fastConfig());

    EXPECT_NE(r.status, FitStatus::InvalidInput);
    EXPECT_NE(r.status, FitStatus::ZeroEnergy);
    EXPECT_TRUE(std::isfinite(r.parameters.m11));
    EXPECT_TRUE(std::isfinite(r.parameters.m22));
    EXPECT_TRUE(std::isfinite(r.parameters.m13));
    EXPECT_GT(r.parameters.m11, 0.0);
    EXPECT_GT(r.parameters.m22, 0.0);
    EXPECT_GT(r.magnitude, 0.0);
}

// ---- Final error does not exceed initial error ----

TEST(LTCFitter, FinalErrorLeInitialError)
{
    GGX brdf;
    BRDFContext ctx{glm::normalize(Vec3(0.4, 0.0, std::sqrt(1.0 - 0.16))), Scalar(0.5)};

    LTCFitter fitter;
    auto r = fitter.fit(brdf, ctx, fastConfig());

    EXPECT_NE(r.status, FitStatus::InvalidInput);
    EXPECT_LE(r.finalError, r.initialError + 1e-12);
}

// ---- Normal incidence: m11 == m22, m13 == 0 (1D optimization path) ----

TEST(LTCFitter, NormalIncidenceSymmetry)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0.0, 0.0, 1.0), Scalar(0.5)};

    LTCFitter fitter;
    FittingConfig cfg = fastConfig();
    auto r = fitter.fit(brdf, ctx, cfg);

    EXPECT_NE(r.status, FitStatus::InvalidInput);
    EXPECT_NEAR(r.parameters.m11, r.parameters.m22, 1e-10);
    EXPECT_NEAR(r.parameters.m13, 0.0, 1e-10);
}

// ---- Resulting LTC matrix is invertible ----

TEST(LTCFitter, ResultMatrixIsInvertible)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0.0, 0.0, 1.0), Scalar(0.4)};

    LTCFitter fitter;
    auto r = fitter.fit(brdf, ctx, fastConfig());

    EXPECT_NE(r.status, FitStatus::InvalidInput);

    // Reconstruct the LTC and verify the inverse exists.
    LTC ltc;
    const auto& p = r.parameters;
    bool ok = ltc.setBasis(Vec3(p.basis[0]), Vec3(p.basis[1]), Vec3(p.basis[2]))
           && ltc.setParameters(p.m11, p.m22, p.m13);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(std::isfinite(ltc.absDetInverse()));
    EXPECT_GT(ltc.absDetInverse(), 0.0);
}

// ---- Invalid context returns InvalidInput ----

TEST(LTCFitter, InvalidContextReturnsError)
{
    GGX brdf;
    BRDFContext bad{Vec3(0.0, 0.0, 1.0), Scalar(-0.1)}; // negative alpha

    LTCFitter fitter;
    auto r = fitter.fit(brdf, bad, fastConfig());

    EXPECT_EQ(r.status, FitStatus::InvalidInput);
}

// ---- Determinism: same inputs → same outputs ----

TEST(LTCFitter, Deterministic)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0.0, 0.0, 1.0), Scalar(0.5)};

    LTCFitter fitter;
    auto r1 = fitter.fit(brdf, ctx, fastConfig());
    auto r2 = fitter.fit(brdf, ctx, fastConfig());

    EXPECT_EQ(r1.status,     r2.status);
    EXPECT_DOUBLE_EQ(r1.parameters.m11, r2.parameters.m11);
    EXPECT_DOUBLE_EQ(r1.parameters.m22, r2.parameters.m22);
    EXPECT_DOUBLE_EQ(r1.parameters.m13, r2.parameters.m13);
    EXPECT_DOUBLE_EQ(r1.finalError,     r2.finalError);
    EXPECT_EQ(r1.evaluations,           r2.evaluations);
}

// ---- Grazing angle: should still produce a valid result ----

TEST(LTCFitter, GrazingAngleValid)
{
    GGX brdf;
    // cos(theta) ≈ 0.1 — near-grazing view.
    Vec3 v = glm::normalize(Vec3(std::sqrt(1.0 - 0.01), 0.0, 0.1));
    BRDFContext ctx{v, Scalar(0.5)};

    LTCFitter fitter;
    auto r = fitter.fit(brdf, ctx, fastConfig());

    // May converge or hit max iters, but should not be invalid/zero-energy.
    EXPECT_NE(r.status, FitStatus::InvalidInput);
    EXPECT_GT(r.magnitude, 0.0);
    EXPECT_TRUE(std::isfinite(r.finalError));
}
