#include "ltc/Sampling.h"
#include <cmath>
#include <stdexcept>

namespace ltc {

// ---- SampleGrid ----

SampleGrid::SampleGrid(uint32_t resolution)
    : m_resolution(resolution)
{
    if (resolution == 0)
        throw std::invalid_argument("SampleGrid: resolution must be > 0");
}

Sample2D SampleGrid::sample(uint32_t x, uint32_t y) const noexcept
{
    return {
        (static_cast<Scalar>(x) + Scalar(0.5)) / static_cast<Scalar>(m_resolution),
        (static_cast<Scalar>(y) + Scalar(0.5)) / static_cast<Scalar>(m_resolution)
    };
}

// ---- Cosine hemisphere ----

DirectionSample sampleCosineHemisphere(const Vec2& u) noexcept
{
    // Malley's method: uniform disk → cosine hemisphere.
    // theta = acos(sqrt(1 - u.x)), but computed stably via sin/cos.
    Scalar cosTheta = std::sqrt(u.x);
    Scalar sinTheta = std::sqrt(Scalar(1) - u.x);
    Scalar phi      = kTwoPi * u.y;

    Vec3 dir(sinTheta * std::cos(phi),
             sinTheta * std::sin(phi),
             cosTheta);

    Scalar p = cosTheta * kInvPi; // cos(theta) / pi
    return {dir, p};
}

Scalar cosineHemispherePdf(const Vec3& direction) noexcept
{
    if (direction.z <= Scalar(0))
        return Scalar(0);
    return direction.z * kInvPi;
}

} // namespace ltc
