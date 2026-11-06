#include "ltc/Beckmann.h"
#include <algorithm>
#include <cmath>

namespace ltc {

namespace {

constexpr Scalar kMinAlpha = 1e-4;

} // anonymous namespace

// ----------------------------------------------------------------------------
// Static sub-components
// ----------------------------------------------------------------------------

Scalar Beckmann::ndf(Scalar alpha, Scalar cosH) noexcept
{
    if (cosH <= Scalar(0) || alpha <= Scalar(0))
        return Scalar(0);

    Scalar cosH2 = cosH * cosH;
    Scalar cosH4 = cosH2 * cosH2;
    Scalar tanH2 = (Scalar(1) - cosH2) / cosH2; // tan^2(theta_h)
    Scalar a2    = alpha * alpha;
    return std::exp(-tanH2 / a2) / (kPi * a2 * cosH4);
}

Scalar Beckmann::smithG1(Scalar alpha, Scalar cosTheta, Scalar sinTheta) noexcept
{
    if (cosTheta <= Scalar(0))
        return Scalar(0);

    // a = cosTheta / (alpha * sinTheta)
    // Guard against division by zero when sinTheta ~ 0 (normal incidence).
    if (sinTheta < Scalar(1e-12))
        return Scalar(1); // at normal incidence, G1 = 1

    Scalar a = cosTheta / (alpha * sinTheta);
    if (a >= Scalar(1.6))
        return Scalar(1);

    Scalar a2 = a * a;
    return (Scalar(3.535) * a + Scalar(2.181) * a2)
         / (Scalar(1) + Scalar(2.276) * a + Scalar(2.577) * a2);
}

// ----------------------------------------------------------------------------
// IBRDF overrides
// ----------------------------------------------------------------------------

std::string_view Beckmann::name() const
{
    return "beckmann";
}

BRDFEvaluation Beckmann::evaluate(const BRDFContext& ctx, const Vec3& light) const
{
    Scalar alpha = std::max(ctx.alpha, kMinAlpha);

    Scalar cosNV = ctx.view.z;
    Scalar cosNL = light.z;
    if (cosNV <= Scalar(0) || cosNL <= Scalar(0))
        return {Scalar(0), Scalar(0)};

    Vec3 H = ctx.view + light;
    Scalar hLen = glm::length(H);
    if (hLen < Scalar(1e-10))
        return {Scalar(0), Scalar(0)};
    H /= hLen;

    Scalar cosNH = H.z;
    Scalar cosVH = glm::dot(ctx.view, H);
    if (cosNH <= Scalar(0) || cosVH <= Scalar(0))
        return {Scalar(0), Scalar(0)};

    Scalar D = ndf(alpha, cosNH);

    Scalar sinNV = std::sqrt(std::max(Scalar(0), Scalar(1) - cosNV * cosNV));
    Scalar sinNL = std::sqrt(std::max(Scalar(0), Scalar(1) - cosNL * cosNL));
    Scalar G = smithG1(alpha, cosNV, sinNV) * smithG1(alpha, cosNL, sinNL);

    // f_r = D * G / (4 * cosNV)   (Fresnel = 1)
    // value = f_r * cosNL
    Scalar value = D * G * cosNL / (Scalar(4) * cosNV);

    // PDF: p_L = D * cosNH / (4 * cosVH)
    Scalar p = D * cosNH / (Scalar(4) * std::max(cosVH, Scalar(1e-7)));

    return {value, p};
}

Vec3 Beckmann::sample(const BRDFContext& ctx, const Vec2& u) const
{
    Scalar alpha = std::max(ctx.alpha, kMinAlpha);

    // Sample Beckmann NDF half-vector:
    //   tan^2(theta_h) = -alpha^2 * ln(max(1e-10, 1 - u.x))
    Scalar logVal  = std::log(std::max(Scalar(1e-10), Scalar(1) - u.x));
    Scalar tanH2   = -alpha * alpha * logVal;
    Scalar cosH    = Scalar(1) / std::sqrt(Scalar(1) + tanH2);
    Scalar sinH    = std::sqrt(std::max(Scalar(0), Scalar(1) - cosH * cosH));
    Scalar phi     = kTwoPi * u.y;

    Vec3 H(sinH * std::cos(phi), sinH * std::sin(phi), cosH);

    // Reflect view about H to get L = 2*(V·H)*H - V
    Scalar dotVH = glm::dot(ctx.view, H);
    Vec3 L = Scalar(2) * dotVH * H - ctx.view;

    // Return L even if it is below the horizon; evaluate() will return 0.
    return L;
}

BRDFCapabilities Beckmann::capabilities() const
{
    BRDFCapabilities c;
    c.isotropic = true;
    c.reciprocal = true;
    c.supportsImportanceSampling = true;
    c.supportsSchlickFresnel = true;
    c.amplitudeModel = AmplitudeModel::SchlickFresnel;
    c.parameterization = BRDFParameterization::MicrofacetAlpha;
    c.minRoughness = 0.0;
    c.maxRoughness = 1.0;
    c.parameterName = "alpha";
    return c;
}

double Beckmann::contextAlpha(double r) const
{
    double a = r * r;
    return a < 1e-5 ? 1e-5 : a;
}

} // namespace ltc
