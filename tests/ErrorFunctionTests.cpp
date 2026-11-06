#include <gtest/gtest.h>
#include "ltc/ErrorFunction.h"
#include "ltc/BRDFIntegration.h"
#include "ltc/GGX.h"
#include "ltc/LTC.h"
#include <cmath>

using namespace ltc;

static FittingConfig smallConfig()
{
    FittingConfig c;
    c.sampleResolution = 16;
    return c;
}

// ---- Adapter: wraps LTC as an IBRDF so we can compare LTC vs itself ----

class LTCBrdf final : public IBRDF
{
public:
    explicit LTCBrdf(const LTC& ltc) : m_ltc(ltc) {}

    BRDFEvaluation evaluate(const BRDFContext&, const Vec3& light) const override
    {
        Scalar v = m_ltc.evaluate(light);
        Scalar p = m_ltc.pdf(light);
        return {v, p};
    }

    std::string_view name() const override { return "ltc_brdf"; }

    Vec3 sample(const BRDFContext&, const Vec2& u) const override
    {
        return m_ltc.sample(u);
    }

    BRDFCapabilities capabilities() const override { return BRDFCapabilities{}; }
    double contextAlpha(double r) const override { return r; }

private:
    const LTC& m_ltc;
};

// ---- Identical distributions yield near-zero error ----

TEST(ErrorFunction, IdenticalDistributionsLowError)
{
    LTC ltc;
    ltc.setParameters(0.5, 0.5, 0.0);
    ltc.setMagnitude(1.0);

    LTCBrdf brdf(ltc);
    BRDFContext ctx{Vec3(0.0, 0.0, 1.0), Scalar(0.5)};

    LTCErrorFunction errFn;
    double err = errFn.evaluate(brdf, ctx, ltc, smallConfig());

    EXPECT_GE(err, 0.0);
    EXPECT_LT(err, 1e-10); // exact same distribution → zero error
}

// ---- Different distributions yield positive error ----

TEST(ErrorFunction, DifferentDistributionsPositiveError)
{
    // BRDF distribution: very narrow LTC
    LTC narrowLtc;
    narrowLtc.setParameters(0.05, 0.05, 0.0);
    narrowLtc.setMagnitude(1.0);

    // Candidate LTC: wide
    LTC wideLtc;
    wideLtc.setParameters(1.0, 1.0, 0.0);
    wideLtc.setMagnitude(1.0);

    LTCBrdf brdf(narrowLtc);
    BRDFContext ctx{Vec3(0.0, 0.0, 1.0), Scalar(0.5)};

    LTCErrorFunction errFn;
    double err = errFn.evaluate(brdf, ctx, wideLtc, smallConfig());

    EXPECT_GT(err, 0.0);
    EXPECT_TRUE(std::isfinite(err));
}

// ---- Error is non-negative ----

TEST(ErrorFunction, ErrorIsNonNegative)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0.0, 0.0, 1.0), Scalar(0.3)};

    LTC ltc;
    ltc.setParameters(0.4, 0.4, 0.0);
    ltc.setMagnitude(0.8);

    LTCErrorFunction errFn;
    double err = errFn.evaluate(brdf, ctx, ltc, smallConfig());

    EXPECT_GE(err, 0.0);
    EXPECT_TRUE(std::isfinite(err));
}

// ---- Determinism ----

TEST(ErrorFunction, Deterministic)
{
    GGX brdf;
    BRDFContext ctx;
    ctx.view  = glm::normalize(Vec3(0.4, 0.0, std::sqrt(1.0 - 0.16)));
    ctx.alpha = Scalar(0.5);

    LTC ltc;
    ltc.setParameters(0.6, 0.4, 0.1);
    ltc.setMagnitude(1.0);

    LTCErrorFunction errFn;
    double e1 = errFn.evaluate(brdf, ctx, ltc, smallConfig());
    double e2 = errFn.evaluate(brdf, ctx, ltc, smallConfig());

    EXPECT_DOUBLE_EQ(e1, e2);
}

// ---- GGX error decreases as LTC gets closer to a reasonable fit ----

TEST(ErrorFunction, BetterParamsGiveLowerError)
{
    GGX brdf;
    BRDFContext ctx{Vec3(0.0, 0.0, 1.0), Scalar(0.5)};

    // Use BRDFIntegration to find the BRDF magnitude for consistent magnitude.
    BRDFIntegrator integrator;
    auto integ = integrator.integrate(brdf, ctx, smallConfig());

    LTC badLtc;
    badLtc.setParameters(10.0, 10.0, 0.0); // very wrong
    badLtc.setMagnitude(integ.magnitude);

    LTC goodLtc;
    goodLtc.setParameters(0.5, 0.5, 0.0); // reasonable
    goodLtc.setMagnitude(integ.magnitude);

    LTCErrorFunction errFn;
    double errBad  = errFn.evaluate(brdf, ctx, badLtc,  smallConfig());
    double errGood = errFn.evaluate(brdf, ctx, goodLtc, smallConfig());

    EXPECT_LT(errGood, errBad);
}
