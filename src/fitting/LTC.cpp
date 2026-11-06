#include "ltc/LTC.h"
#include "ltc/Sampling.h"
#include <cmath>
#include <algorithm>

namespace ltc {

// ----------------------------------------------------------------------------
// Construction / defaults
// ----------------------------------------------------------------------------

LTC::LTC()
    : m_X(Scalar(1), Scalar(0), Scalar(0))
    , m_Y(Scalar(0), Scalar(1), Scalar(0))
    , m_Z(Scalar(0), Scalar(0), Scalar(1))
    , m_m11(Scalar(1))
    , m_m22(Scalar(1))
    , m_m13(Scalar(0))
    , m_magnitude(Scalar(1))
    , m_M(Mat3(Scalar(1)))
    , m_Minv(Mat3(Scalar(1)))
    , m_absDetInv(Scalar(1))
{
}

// ----------------------------------------------------------------------------
// Parameter setters
// ----------------------------------------------------------------------------

bool LTC::setParameters(Scalar m11, Scalar m22, Scalar m13) noexcept
{
    // Save current state so we can roll back on failure.
    Scalar old11 = m_m11, old22 = m_m22, old13 = m_m13;
    m_m11 = m11;
    m_m22 = m22;
    m_m13 = m13;
    if (!updateMatrices()) {
        m_m11 = old11;
        m_m22 = old22;
        m_m13 = old13;
        updateMatrices(); // restore — cannot fail (previous state was valid)
        return false;
    }
    return true;
}

bool LTC::setBasis(const Vec3& x, const Vec3& y, const Vec3& z) noexcept
{
    constexpr Scalar kOrthogonalityTol = Scalar(1e-6);

    // Require unit length.
    if (std::abs(glm::length(x) - Scalar(1)) > kOrthogonalityTol ||
        std::abs(glm::length(y) - Scalar(1)) > kOrthogonalityTol ||
        std::abs(glm::length(z) - Scalar(1)) > kOrthogonalityTol)
        return false;

    // Require mutual orthogonality.
    if (std::abs(glm::dot(x, y)) > kOrthogonalityTol ||
        std::abs(glm::dot(x, z)) > kOrthogonalityTol ||
        std::abs(glm::dot(y, z)) > kOrthogonalityTol)
        return false;

    Vec3 oldX = m_X, oldY = m_Y, oldZ = m_Z;
    m_X = x; m_Y = y; m_Z = z;
    if (!updateMatrices()) {
        m_X = oldX; m_Y = oldY; m_Z = oldZ;
        updateMatrices();
        return false;
    }
    return true;
}

void LTC::setMagnitude(Scalar magnitude) noexcept
{
    m_magnitude = magnitude;
}

// ----------------------------------------------------------------------------
// Private matrix update
// ----------------------------------------------------------------------------

bool LTC::updateMatrices() noexcept
{
    // Reference LTC.h column construction (selfshadow/ltc_code):
    //   M[col0] = X*m11 + Z*m13
    //   M[col1] = Y*m22
    //   M[col2] = Z
    //
    // Mathematical representation with identity basis:
    //   [[m11, 0,   0  ],
    //    [0,   m22, 0  ],
    //    [m13, 0,   1  ]]
    Vec3 col0 = m_X * m_m11 + m_Z * m_m13;
    Vec3 col1 = m_Y * m_m22;
    Vec3 col2 = m_Z;

    // GLM Mat3 constructor takes columns.
    Mat3 M(col0, col1, col2);

    auto inv = safeInverse(M);
    if (!inv)
        return false;

    m_M         = M;
    m_Minv      = *inv;
    m_absDetInv = Scalar(1) / std::abs(determinant(M));
    return true;
}

// ----------------------------------------------------------------------------
// Evaluation and sampling
// ----------------------------------------------------------------------------

Scalar LTC::pdf(const Vec3& direction) const noexcept
{
    // Transform direction back through M^{-1}.
    Vec3   Lo  = m_Minv * direction;
    Scalar len = glm::length(Lo);
    if (len < Scalar(1e-15))
        return Scalar(0);

    Vec3 L_ = Lo / len; // normalized original-space direction

    // Clamped cosine in original space.
    Scalar D_cos = std::max(Scalar(0), L_.z) * kInvPi;
    if (D_cos == Scalar(0))
        return Scalar(0);

    // Change-of-variables Jacobian:  |det(M^{-1})| / ||M^{-1}*w||^3
    Scalar jacobian = m_absDetInv / (len * len * len);
    return D_cos * jacobian;
}

Scalar LTC::evaluate(const Vec3& direction) const noexcept
{
    return m_magnitude * pdf(direction);
}

Vec3 LTC::sample(const Vec2& u) const noexcept
{
    // Sample from D_cos using cosine-hemisphere sampling.
    Scalar cosTheta = std::sqrt(u.x);
    Scalar sinTheta = std::sqrt(std::max(Scalar(0), Scalar(1) - u.x));
    Scalar phi      = kTwoPi * u.y;

    Vec3 L0(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);

    // Transform through M and normalize.
    Vec3   L   = m_M * L0;
    Scalar len = glm::length(L);
    if (len < Scalar(1e-15))
        return Vec3(Scalar(0), Scalar(0), Scalar(1));
    return L / len;
}

} // namespace ltc
