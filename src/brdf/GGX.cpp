#include "ltc/GGX.h"
#include <cmath>
#include <algorithm>

namespace ltc {

namespace {

// Minimum alpha used to guard against degenerate NDF.
// Matches the reference fitting configuration (effectively a delta function
// at very low roughness is clamped).
constexpr Scalar kMinAlpha = 1e-4;

} // anonymous namespace

// ----------------------------------------------------------------------------
// Static sub-components
// ----------------------------------------------------------------------------

Scalar GGX::ndf(Scalar alpha, Scalar cosH) noexcept
{
    if (cosH <= Scalar(0) || alpha <= Scalar(0))
        return Scalar(0);

    Scalar a2    = alpha * alpha;
    Scalar cosH2 = cosH * cosH;
    Scalar denom = cosH2 * (a2 - Scalar(1)) + Scalar(1);
    return a2 / (kPi * denom * denom);
}

Scalar GGX::smithG1(Scalar alpha, Scalar cosV) noexcept
{
    if (cosV <= Scalar(0))
        return Scalar(0);

    Scalar a2    = alpha * alpha;
    Scalar cosV2 = cosV * cosV;
    return Scalar(2) * cosV / (cosV + std::sqrt(a2 + (Scalar(1) - a2) * cosV2));
}

Scalar GGX::pdf(Scalar alpha, const Vec3& view, const Vec3& light) noexcept
{
    // N = (0,0,1) in local shading space.
    Scalar cosV = view.z;
    Scalar cosL = light.z;
    if (cosV <= Scalar(0) || cosL <= Scalar(0))
        return Scalar(0);

    Vec3 H = view + light;
    Scalar hLen = glm::length(H);
    if (hLen < Scalar(1e-10))
        return Scalar(0);
    H /= hLen;

    Scalar cosH  = H.z;
    Scalar dotVH = glm::dot(view, H);
    if (dotVH <= Scalar(0) || cosH <= Scalar(0))
        return Scalar(0);

    Scalar D = ndf(alpha, cosH);
    // Transform half-vector PDF to reflected-direction PDF:
    //   p_L = D(H) * (N·H) / (4 * |V·H|)
    return D * cosH / (Scalar(4) * dotVH);
}

// ----------------------------------------------------------------------------
// IBRDF overrides
// ----------------------------------------------------------------------------

std::string_view GGX::name() const
{
    return "ggx";
}

BRDFCapabilities GGX::capabilities() const
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

double GGX::contextAlpha(double r) const
{
    double a = r * r;
    return a < 1e-5 ? 1e-5 : a;
}

BRDFEvaluation GGX::evaluate(const BRDFContext& ctx, const Vec3& light) const
{
    Scalar alpha = std::max(ctx.alpha, kMinAlpha);

    Scalar cosV = ctx.view.z;
    Scalar cosL = light.z;
    if (cosV <= Scalar(0) || cosL <= Scalar(0))
        return {Scalar(0), Scalar(0)};

    Vec3 H = ctx.view + light;
    Scalar hLen = glm::length(H);
    if (hLen < Scalar(1e-10))
        return {Scalar(0), Scalar(0)};
    H /= hLen;

    Scalar cosH  = H.z;
    Scalar dotVH = glm::dot(ctx.view, H);
    if (dotVH <= Scalar(0) || cosH <= Scalar(0))
        return {Scalar(0), Scalar(0)};

    Scalar D  = ndf(alpha, cosH);
    Scalar G  = smithG1(alpha, cosV) * smithG1(alpha, cosL);

    // f_r(V,L) = D*G / (4*cosV)   (Fresnel = 1)
    // value    = f_r * cosL
    Scalar value = D * G * cosL / (Scalar(4) * cosV);

    // PDF: p_L = D(H)*(N·H) / (4*|V·H|)
    Scalar p = D * cosH / (Scalar(4) * dotVH);

    return {value, p};
}

Vec3 GGX::sample(const BRDFContext& ctx, const Vec2& u) const
{
    Scalar alpha = std::max(ctx.alpha, kMinAlpha);

    // Sample the GGX NDF half-vector.
    // theta_H = atan(alpha * sqrt(u1 / (1 - u1)))
    Scalar tanTheta = alpha * std::sqrt(u.x / std::max(Scalar(1) - u.x, Scalar(1e-30)));
    Scalar cosH     = Scalar(1) / std::sqrt(Scalar(1) + tanTheta * tanTheta);
    Scalar sinH     = tanTheta * cosH;
    Scalar phi      = kTwoPi * u.y;

    // Half-vector in local (z-up) shading space.
    Vec3 H(sinH * std::cos(phi), sinH * std::sin(phi), cosH);

    // Reflect view about H to obtain L = 2*(V·H)*H - V.
    Scalar dotVH = glm::dot(ctx.view, H);
    Vec3 L       = Scalar(2) * dotVH * H - ctx.view;

    // The reflected direction may fall below the horizon.
    // We return it as-is; the caller (evaluate / pdf) will return 0 for it.
    return L;
}

} // namespace ltc
