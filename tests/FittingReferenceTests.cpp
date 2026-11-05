#include <gtest/gtest.h>
#include "ltc/Fitter.h"
#include "ltc/GGX.h"
#include "ltc/LTC.h"
#include "ltc/Math.h"
#include <cmath>
#include <array>

using namespace ltc;

// These tests verify structural sanity of the fitted parameters against
// properties that hold for all valid GGX fittings.

struct FitCase
{
    double alpha;
    double ndotv; // cos(theta_view)
};

static const std::array<FitCase, 6> kTestCases = {{
    {0.1,  1.0},  // smooth, normal incidence
    {0.5,  1.0},  // mid roughness, normal incidence
    {0.9,  1.0},  // rough, normal incidence
    {0.3,  0.7},  // smooth-ish, oblique
    {0.5,  0.5},  // mid, mid-angle
    {0.7,  0.3},  // rough, near-grazing
}};

static FittingConfig referenceConfig()
{
    FittingConfig c;
    c.sampleResolution = 32;
    c.maxIterations    = 100;
    return c;
}

// ---- Magnitude in (0, pi] for valid GGX inputs ----

TEST(FittingReference, MagnitudeInValidRange)
{
    GGX brdf;
    LTCFitter fitter;

    for (const auto& tc : kTestCases) {
        double sinv = std::sqrt(std::max(0.0, 1.0 - tc.ndotv * tc.ndotv));
        Vec3 v = glm::normalize(Vec3(sinv, 0.0, tc.ndotv));
        BRDFContext ctx{v, Scalar(tc.alpha)};

        auto r = fitter.fit(brdf, ctx, referenceConfig());
        SCOPED_TRACE("alpha=" + std::to_string(tc.alpha) + " ndotv=" + std::to_string(tc.ndotv));

        EXPECT_NE(r.status, FitStatus::InvalidInput);
        EXPECT_NE(r.status, FitStatus::ZeroEnergy);
        EXPECT_GT(r.magnitude, 0.0);
        EXPECT_LE(r.magnitude, 3.15); // <= pi with some tolerance
    }
}

// ---- m11 and m22 are always positive ----

TEST(FittingReference, ScaleParametersPositive)
{
    GGX brdf;
    LTCFitter fitter;

    for (const auto& tc : kTestCases) {
        double sinv = std::sqrt(std::max(0.0, 1.0 - tc.ndotv * tc.ndotv));
        Vec3 v = glm::normalize(Vec3(sinv, 0.0, tc.ndotv));
        BRDFContext ctx{v, Scalar(tc.alpha)};

        auto r = fitter.fit(brdf, ctx, referenceConfig());
        SCOPED_TRACE("alpha=" + std::to_string(tc.alpha) + " ndotv=" + std::to_string(tc.ndotv));

        EXPECT_NE(r.status, FitStatus::InvalidInput);
        EXPECT_GT(r.parameters.m11, 0.0);
        EXPECT_GT(r.parameters.m22, 0.0);
    }
}

// ---- Resulting LTC matrix is always invertible ----

TEST(FittingReference, MatrixIsInvertible)
{
    GGX brdf;
    LTCFitter fitter;

    for (const auto& tc : kTestCases) {
        double sinv = std::sqrt(std::max(0.0, 1.0 - tc.ndotv * tc.ndotv));
        Vec3 v = glm::normalize(Vec3(sinv, 0.0, tc.ndotv));
        BRDFContext ctx{v, Scalar(tc.alpha)};

        auto r = fitter.fit(brdf, ctx, referenceConfig());
        SCOPED_TRACE("alpha=" + std::to_string(tc.alpha) + " ndotv=" + std::to_string(tc.ndotv));

        EXPECT_NE(r.status, FitStatus::InvalidInput);

        LTC ltc;
        const auto& p = r.parameters;
        bool ok = ltc.setBasis(Vec3(p.basis[0]), Vec3(p.basis[1]), Vec3(p.basis[2]))
               && ltc.setParameters(p.m11, p.m22, p.m13);
        EXPECT_TRUE(ok);
        EXPECT_GT(ltc.absDetInverse(), 0.0);
    }
}

// ---- Optimization reduces error ----

TEST(FittingReference, ErrorReducedFromInitialGuess)
{
    GGX brdf;
    LTCFitter fitter;

    for (const auto& tc : kTestCases) {
        double sinv = std::sqrt(std::max(0.0, 1.0 - tc.ndotv * tc.ndotv));
        Vec3 v = glm::normalize(Vec3(sinv, 0.0, tc.ndotv));
        BRDFContext ctx{v, Scalar(tc.alpha)};

        auto r = fitter.fit(brdf, ctx, referenceConfig());
        SCOPED_TRACE("alpha=" + std::to_string(tc.alpha) + " ndotv=" + std::to_string(tc.ndotv));

        EXPECT_NE(r.status, FitStatus::InvalidInput);
        EXPECT_LE(r.finalError, r.initialError + 1e-10);
    }
}

// ---- Fresnel coefficient in [0, 1] when present ----

TEST(FittingReference, FresnelCoefficientInRange)
{
    GGX brdf;
    LTCFitter fitter;

    for (const auto& tc : kTestCases) {
        double sinv = std::sqrt(std::max(0.0, 1.0 - tc.ndotv * tc.ndotv));
        Vec3 v = glm::normalize(Vec3(sinv, 0.0, tc.ndotv));
        BRDFContext ctx{v, Scalar(tc.alpha)};

        auto r = fitter.fit(brdf, ctx, referenceConfig());
        SCOPED_TRACE("alpha=" + std::to_string(tc.alpha) + " ndotv=" + std::to_string(tc.ndotv));

        if (r.hasFresnelCoefficient) {
            EXPECT_GE(r.fresnelCoefficient, 0.0);
            EXPECT_LE(r.fresnelCoefficient, 1.0 + 1e-6);
        }
    }
}

// ---- Normal incidence: 1D path produces symmetric matrix ----

TEST(FittingReference, NormalIncidenceIsSymmetric)
{
    GGX brdf;
    LTCFitter fitter;

    for (double alpha : {0.1, 0.3, 0.5, 0.7, 0.9}) {
        BRDFContext ctx{Vec3(0.0, 0.0, 1.0), Scalar(alpha)};
        auto r = fitter.fit(brdf, ctx, referenceConfig());
        SCOPED_TRACE("alpha=" + std::to_string(alpha));

        EXPECT_NE(r.status, FitStatus::InvalidInput);
        EXPECT_NEAR(r.parameters.m11, r.parameters.m22, 1e-10);
        EXPECT_NEAR(r.parameters.m13, 0.0, 1e-10);
    }
}
