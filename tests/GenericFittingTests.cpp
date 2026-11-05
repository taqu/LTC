#include <gtest/gtest.h>
#include "ltc/Beckmann.h"
#include "ltc/DisneyDiffuse.h"
#include "ltc/Fitter.h"
#include "ltc/GGX.h"
#include "ltc/Math.h"
#include <cmath>

using namespace ltc;

// A minimal Lambertian BRDF to test extensibility without registry changes.
struct Lambertian final : public IBRDF {
    std::string_view name() const override { return "lambertian"; }

    BRDFCapabilities capabilities() const override {
        BRDFCapabilities c;
        c.amplitudeModel    = AmplitudeModel::Scalar;
        c.supportsSchlickFresnel = false;
        c.parameterization  = BRDFParameterization::PerceptualRoughness;
        return c;
    }

    double contextAlpha(double r) const override { return r; }

    BRDFEvaluation evaluate(const BRDFContext&, const Vec3& L) const override {
        double cosNL = std::max(0.0, L.z);
        return {cosNL * kInvPi, cosNL * kInvPi};
    }

    Vec3 sample(const BRDFContext&, const Vec2& u) const override {
        double cosT = std::sqrt(u.x);
        double sinT = std::sqrt(std::max(0.0, 1.0 - u.x));
        double phi  = kTwoPi * u.y;
        return Vec3(sinT * std::cos(phi), sinT * std::sin(phi), cosT);
    }
};

// ---- Beckmann single-point fit ----

TEST(GenericFitting, BeckmannFitConvergesOrMaxIter)
{
    Beckmann brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    FittingConfig cfg;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 200;

    LTCFitter fitter;
    FittingResult r = fitter.fit(brdf, ctx, cfg);

    EXPECT_TRUE(r.status == FitStatus::Converged ||
                r.status == FitStatus::MaxIterations);
    EXPECT_GT(r.magnitude, 0.0);
    EXPECT_TRUE(std::isfinite(r.parameters.m11));
    EXPECT_TRUE(std::isfinite(r.parameters.m22));
    EXPECT_TRUE(std::isfinite(r.parameters.m13));
    EXPECT_LE(r.finalError, r.initialError + 1e-6);
}

// ---- Disney Diffuse single-point fit ----

TEST(GenericFitting, DisneyDiffuseFitConvergesOrMaxIter)
{
    DisneyDiffuse brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    FittingConfig cfg;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 200;

    LTCFitter fitter;
    FittingResult r = fitter.fit(brdf, ctx, cfg);

    EXPECT_TRUE(r.status == FitStatus::Converged ||
                r.status == FitStatus::MaxIterations);
    EXPECT_GT(r.magnitude, 0.0);
    EXPECT_TRUE(std::isfinite(r.parameters.m11));
    EXPECT_TRUE(std::isfinite(r.parameters.m22));
    EXPECT_TRUE(std::isfinite(r.parameters.m13));
}

// ---- GGX regression test ----

TEST(GenericFitting, GGXRegressionFit)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    FittingConfig cfg;
    cfg.sampleResolution = 32;
    cfg.maxIterations    = 200;

    LTCFitter fitter;
    FittingResult r = fitter.fit(brdf, ctx, cfg);

    EXPECT_TRUE(r.status == FitStatus::Converged ||
                r.status == FitStatus::MaxIterations);
    EXPECT_GT(r.magnitude, 0.0);
    EXPECT_TRUE(std::isfinite(r.parameters.m11));
    EXPECT_TRUE(std::isfinite(r.parameters.m22));
    EXPECT_TRUE(std::isfinite(r.parameters.m13));
}

// ---- Lambertian custom BRDF fit ----

TEST(GenericFitting, LambertianFitApproxIdentity)
{
    // The LTC for a Lambertian BRDF at normal incidence should be close to
    // the identity matrix (m11 ≈ 1, m22 ≈ 1, m13 ≈ 0) and magnitude ≈ 1.
    Lambertian brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    FittingConfig cfg;
    cfg.sampleResolution = 64;
    cfg.maxIterations    = 200;

    LTCFitter fitter;
    FittingResult r = fitter.fit(brdf, ctx, cfg);

    EXPECT_TRUE(r.status == FitStatus::Converged ||
                r.status == FitStatus::MaxIterations);
    EXPECT_TRUE(std::isfinite(r.magnitude));
    EXPECT_GT(r.magnitude, 0.0);
    // The Lambertian magnitude should be close to 1 (integrates cosNL/pi over hemisphere = 1)
    EXPECT_NEAR(r.magnitude, 1.0, 0.05);
    // Near-identity parameters at normal incidence
    EXPECT_NEAR(r.parameters.m11, 1.0, 0.1);
    EXPECT_NEAR(r.parameters.m22, 1.0, 0.1);
    EXPECT_NEAR(r.parameters.m13, 0.0, 0.1);
}

// ---- contextAlpha cross-check ----

TEST(GenericFitting, ContextAlphaGGXPreservesExistingBehavior)
{
    GGX brdf;
    // contextAlpha for GGX = max(r^2, 1e-5)
    EXPECT_NEAR(brdf.contextAlpha(0.5), 0.25, 1e-15);
    EXPECT_NEAR(brdf.contextAlpha(0.0), 1e-5, 1e-20);
    EXPECT_NEAR(brdf.contextAlpha(1.0), 1.0,  1e-15);
}

TEST(GenericFitting, ContextAlphaDisneyDiffuseNoSquaring)
{
    DisneyDiffuse brdf;
    EXPECT_DOUBLE_EQ(brdf.contextAlpha(0.5), 0.5);
    EXPECT_DOUBLE_EQ(brdf.contextAlpha(0.25), 0.25);
}
