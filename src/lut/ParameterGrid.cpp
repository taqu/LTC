#include "ltc/ParameterGrid.h"
#include <cmath>
#include <stdexcept>

namespace ltc {

ParameterGrid::ParameterGrid(uint32_t width, uint32_t height)
    : m_width(width), m_height(height)
{
    if (width  < 2) throw std::invalid_argument("ParameterGrid: width must be >= 2");
    if (height < 2) throw std::invalid_argument("ParameterGrid: height must be >= 2");
    // Sanity-check total cell count won't overflow size_t on 32-bit.
    if (static_cast<uint64_t>(width) * height > 0x40000000ULL)
        throw std::invalid_argument("ParameterGrid: grid too large");
}

double ParameterGrid::roughnessAt(uint32_t a) const noexcept
{
    return static_cast<double>(a) / static_cast<double>(m_width - 1);
}

double ParameterGrid::alphaAt(uint32_t a) const noexcept
{
    double r = roughnessAt(a);
    double alpha = r * r;
    return alpha < kMinAlpha ? kMinAlpha : alpha;
}

double ParameterGrid::cosThetaAt(uint32_t t) const noexcept
{
    double x = static_cast<double>(t) / static_cast<double>(m_height - 1);
    double cosTheta = 1.0 - x * x;
    // Clamp to valid range due to floating-point arithmetic.
    if (cosTheta < 0.0) cosTheta = 0.0;
    if (cosTheta > 1.0) cosTheta = 1.0;
    return cosTheta;
}

double ParameterGrid::thetaAt(uint32_t t) const noexcept
{
    double cosTheta = cosThetaAt(t);
    double theta = std::acos(cosTheta);
    // Reference parameterization: grazing angle clamped to 1.57 (< pi/2).
    constexpr double kMaxTheta = 1.57;
    return theta < kMaxTheta ? theta : kMaxTheta;
}

Vec3 ParameterGrid::viewAt(uint32_t t) const noexcept
{
    double theta = thetaAt(t);
    double sinT  = std::sin(theta);
    double cosT  = std::cos(theta);
    return Vec3(sinT, 0.0, cosT);
}

LUTParameters ParameterGrid::evaluate(LUTCoordinates coords) const
{
    LUTParameters p;
    p.perceptualRoughness = roughnessAt(coords.roughnessIndex);
    p.alpha               = alphaAt(coords.roughnessIndex);
    p.ndotv               = cosThetaAt(coords.viewIndex);
    p.theta               = thetaAt(coords.viewIndex);
    p.viewDirection       = viewAt(coords.viewIndex);
    return p;
}

size_t ParameterGrid::index(LUTCoordinates coords) const
{
    return static_cast<size_t>(coords.roughnessIndex)
         + static_cast<size_t>(coords.viewIndex) * m_width;
}

} // namespace ltc
