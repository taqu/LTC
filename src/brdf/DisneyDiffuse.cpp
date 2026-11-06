#include "ltc/DisneyDiffuse.h"
#include <cmath>

namespace ltc {

namespace {

inline double pow5(double x) noexcept
{
    double x2 = x * x;
    return x2 * x2 * x;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// IBRDF overrides
// ----------------------------------------------------------------------------

std::string_view DisneyDiffuse::name() const
{
    return "disney-diffuse";
}

BRDFEvaluation DisneyDiffuse::evaluate(const BRDFContext& ctx, const Vec3& light) const
{
    Scalar cosNL = light.z;
    Scalar cosNV = ctx.view.z;

    // PDF from cosine-weighted hemisphere sampling.
    Scalar pdf = cosNL > Scalar(0) ? cosNL * kInvPi : Scalar(0);

    if (cosNL <= Scalar(0) || cosNV <= Scalar(0))
        return {Scalar(0), pdf};

    Vec3 sum = ctx.view + light;
    Scalar sumLen = glm::length(sum);
    Scalar cosLH;
    if (sumLen < Scalar(1e-10)) {
        cosLH = Scalar(0);
    } else {
        Vec3 H = sum / sumLen;
        cosLH  = std::max(Scalar(0), glm::dot(light, H));
    }

    Scalar r    = ctx.alpha; // perceptual roughness used directly
    Scalar FD90 = Scalar(0.5) + Scalar(2.0) * r * cosLH * cosLH;
    Scalar FL   = pow5(Scalar(1) - cosNL);
    Scalar FV   = pow5(Scalar(1) - cosNV);
    Scalar fd   = kInvPi * (Scalar(1) + (FD90 - Scalar(1)) * FL)
                         * (Scalar(1) + (FD90 - Scalar(1)) * FV);

    Scalar value = fd * cosNL;

    return {value, pdf};
}

Vec3 DisneyDiffuse::sample(const BRDFContext& /*ctx*/, const Vec2& u) const
{
    // Cosine-weighted hemisphere sampling.
    Scalar cosT = std::sqrt(u.x);
    Scalar sinT = std::sqrt(std::max(Scalar(0), Scalar(1) - u.x));
    Scalar phi  = kTwoPi * u.y;
    return Vec3(sinT * std::cos(phi), sinT * std::sin(phi), cosT);
}

BRDFCapabilities DisneyDiffuse::capabilities() const
{
    BRDFCapabilities c;
    c.isotropic = true;
    c.reciprocal = true;
    c.supportsImportanceSampling = true;
    c.supportsSchlickFresnel = false;
    c.amplitudeModel = AmplitudeModel::Scalar;
    c.parameterization = BRDFParameterization::PerceptualRoughness;
    c.minRoughness = 0.0;
    c.maxRoughness = 1.0;
    c.parameterName = "roughness";
    return c;
}

double DisneyDiffuse::contextAlpha(double perceptualRoughness) const
{
    return perceptualRoughness;
}

} // namespace ltc
