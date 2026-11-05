#include <gtest/gtest.h>
#include "ltc/BRDF.h"
#include "ltc/GGX.h"

using namespace ltc;

// A minimal BRDF that always returns a fixed value — used to verify the
// interface contract independently of GGX.
class ConstantBRDF final : public IBRDF {
public:
    std::string_view name() const override { return "Constant"; }

    BRDFEvaluation evaluate(const BRDFContext&, const Vec3&) const override {
        return {0.5, 0.25};
    }

    Vec3 sample(const BRDFContext&, const Vec2&) const override {
        return Vec3(0, 0, 1);
    }

    BRDFCapabilities capabilities() const override {
        return BRDFCapabilities{};
    }

    double contextAlpha(double r) const override { return r; }
};

TEST(BRDFInterface, NameIsAccessible)
{
    ConstantBRDF brdf;
    EXPECT_EQ(brdf.name(), "Constant");
}

TEST(BRDFInterface, EvaluateReturnsExpectedFields)
{
    ConstantBRDF brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    auto result = brdf.evaluate(ctx, Vec3(0, 0, 1));
    EXPECT_DOUBLE_EQ(result.value, 0.5);
    EXPECT_DOUBLE_EQ(result.pdf, 0.25);
}

TEST(BRDFInterface, SampleReturnsDirection)
{
    ConstantBRDF brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    Vec3 dir = brdf.sample(ctx, Vec2(0.5, 0.5));
    EXPECT_DOUBLE_EQ(dir.z, 1.0);
}

TEST(BRDFInterface, GGXImplementsInterface)
{
    // GGX is an IBRDF and can be used polymorphically.
    std::unique_ptr<IBRDF> brdf = std::make_unique<GGX>();
    EXPECT_EQ(brdf->name(), "ggx");

    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    auto result = brdf->evaluate(ctx, Vec3(0, 0, 1));
    EXPECT_GE(result.value, 0.0);
    EXPECT_GE(result.pdf, 0.0);
}

TEST(BRDFInterface, ValueAndPDFAreIndependent)
{
    // The value field should NOT contain the PDF.
    // We verify by checking that evaluate at two different directions returns
    // different PDFs but we cannot derive the value from the PDF directly.
    GGX brdf;
    BRDFContext ctx{Vec3(0, 0, 1), 0.5};
    auto r1 = brdf.evaluate(ctx, Vec3(0, 0, 1));
    auto r2 = brdf.evaluate(ctx, Vec3(0.5, 0, std::sqrt(0.75)));

    // Both should be valid.
    EXPECT_GE(r1.value, 0.0);
    EXPECT_GE(r1.pdf, 0.0);
    EXPECT_GE(r2.value, 0.0);
    EXPECT_GE(r2.pdf, 0.0);

    // The two directions should generally give different PDFs.
    // (They would only be equal by coincidence for a specific alpha.)
    EXPECT_NE(r1.pdf, r2.pdf);
}
