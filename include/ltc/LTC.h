#pragma once
#include "Math.h"

namespace ltc {

// Linearly Transformed Cosines (LTC) distribution.
//
// The base distribution is the clamped cosine:
//   D_cos(w) = max(0, w.z) / pi
//
// The LTC distribution is obtained by transforming directions through an
// invertible 3x3 matrix M:
//   D_LTC(w; M) = D_cos(M^{-1}*w / ||M^{-1}*w||) * |det(M^{-1})| / ||M^{-1}*w||^3
//
// Matrix parameterization (reference-compatible, selfshadow/ltc_code):
//   M = [X*m11 + Z*m13, Y*m22, Z]  (columns in GLM convention)
//
// In mathematical column-vector notation with identity basis (X,Y,Z = e1,e2,e3):
//   M = [[m11, 0,   0  ],
//        [0,   m22, 0  ],
//        [m13, 0,   1  ]]
//
// The magnitude parameter scales the evaluated distribution but does NOT
// affect the PDF or sampled directions.
class LTC {
public:
    // Default-constructs an identity LTC: M = I, magnitude = 1.
    LTC();

    // Set the three scalar parameters that define the matrix shape.
    // Returns false and leaves the LTC unchanged when the resulting matrix
    // would be singular (e.g., m11 == 0 or m22 == 0).
    bool setParameters(Scalar m11, Scalar m22, Scalar m13) noexcept;

    // Set the orthonormal basis.  Returns false when any axis is zero-length
    // or the three vectors are not mutually orthogonal (tolerance 1e-6).
    bool setBasis(const Vec3& x, const Vec3& y, const Vec3& z) noexcept;

    // Set the amplitude multiplier.  Negative values are accepted.
    void setMagnitude(Scalar magnitude) noexcept;

    // ---- Query ----

    // Normalized PDF over the full sphere (no magnitude factor).
    // Returns 0 for directions that map to w.z <= 0 in original space.
    Scalar pdf(const Vec3& direction) const noexcept;

    // Amplitude-scaled evaluation: magnitude * pdf(direction).
    Scalar evaluate(const Vec3& direction) const noexcept;

    // Sample a direction from the distribution.
    // u must be in [0, 1)^2.
    Vec3 sample(const Vec2& u) const noexcept;

    const Mat3& matrix()        const noexcept { return m_M;    }
    const Mat3& inverseMatrix() const noexcept { return m_Minv; }
    Scalar      magnitude()     const noexcept { return m_magnitude; }

    // |det(M^{-1})| — used in the PDF Jacobian.
    Scalar absDetInverse() const noexcept { return m_absDetInv; }

    Scalar m11() const noexcept { return m_m11; }
    Scalar m22() const noexcept { return m_m22; }
    Scalar m13() const noexcept { return m_m13; }

    const Vec3& basisX() const noexcept { return m_X; }
    const Vec3& basisY() const noexcept { return m_Y; }
    const Vec3& basisZ() const noexcept { return m_Z; }

private:
    // Recompute m_M, m_Minv, m_absDetInv from the current parameters.
    // Returns false if the matrix is singular.
    bool updateMatrices() noexcept;

    Vec3   m_X, m_Y, m_Z;
    Scalar m_m11, m_m22, m_m13;
    Scalar m_magnitude;
    Mat3   m_M, m_Minv;
    Scalar m_absDetInv; // |det(M^{-1})| = 1 / |det(M)|
};

} // namespace ltc
