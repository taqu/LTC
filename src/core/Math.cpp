#include "ltc/Math.h"
#include <cmath>

namespace ltc {

std::optional<Vec3> safeNormalize(const Vec3& v, Scalar threshold) noexcept
{
    Scalar len = glm::length(v);
    if (len < threshold)
        return std::nullopt;
    return v / len;
}

void buildOrthonormalBasis(const Vec3& n, Vec3& tangent, Vec3& bitangent) noexcept
{
    // Frisvad / Duff et al. 2017 — numerically stable branch-free basis.
    Scalar sign = std::copysign(Scalar(1), n.z);
    Scalar a    = Scalar(-1) / (sign + n.z);
    Scalar b    = n.x * n.y * a;
    tangent     = Vec3(Scalar(1) + sign * n.x * n.x * a, sign * b, -sign * n.x);
    bitangent   = Vec3(b, sign + n.y * n.y * a, -n.y);
}

std::optional<Mat3> safeInverse(const Mat3& m, Scalar threshold) noexcept
{
    Scalar det = glm::determinant(m);
    if (std::abs(det) < threshold)
        return std::nullopt;
    return glm::inverse(m);
}

Scalar determinant(const Mat3& m) noexcept
{
    return glm::determinant(m);
}

bool isFinite(const Vec2& v) noexcept
{
    return std::isfinite(v.x) && std::isfinite(v.y);
}

bool isFinite(const Vec3& v) noexcept
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool isFinite(const Mat3& m) noexcept
{
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
            if (!std::isfinite(m[col][row]))
                return false;
    return true;
}

} // namespace ltc
