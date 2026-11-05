#pragma once
#include <cmath>
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace ltc {

// Scalar type used throughout the mathematical core.
// All BRDF and LTC computations use double precision.
using Scalar = double;
using Vec2   = glm::dvec2;
using Vec3   = glm::dvec3;
using Mat3   = glm::dmat3;

inline constexpr Scalar kPi    = 3.14159265358979323846;
inline constexpr Scalar kInvPi = 1.0 / kPi;
inline constexpr Scalar kTwoPi = 2.0 * kPi;

// Returns the normalized vector, or nullopt when the length is below threshold.
std::optional<Vec3> safeNormalize(const Vec3& v, Scalar threshold = 1e-10) noexcept;

// Constructs a right-handed orthonormal basis {tangent, bitangent} given a
// unit normal n (n must already be normalized).
void buildOrthonormalBasis(const Vec3& n, Vec3& tangent, Vec3& bitangent) noexcept;

// Returns the inverse of m, or nullopt when the matrix is singular
// (|det(m)| < threshold).
std::optional<Mat3> safeInverse(const Mat3& m, Scalar threshold = 1e-15) noexcept;

// Returns the determinant of a 3x3 matrix.
Scalar determinant(const Mat3& m) noexcept;

// Return true when all components are finite (not NaN, not Inf).
bool isFinite(const Vec2& v) noexcept;
bool isFinite(const Vec3& v) noexcept;
bool isFinite(const Mat3& m) noexcept;

} // namespace ltc
