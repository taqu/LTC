#include "ltc/LUTPacking.h"
#include "ltc/Math.h"
#include <gtest/gtest.h>
#include <cmath>
#include <stdexcept>

namespace {

// Helper: build GLM dmat3 from 9 elements in row-major order (for test readability)
// GLM is column-major: m[col][row]
static ltc::Mat3 makeMat3RowMajor(
    double m00, double m01, double m02,
    double m10, double m11, double m12,
    double m20, double m21, double m22)
{
    ltc::Mat3 m;
    // GLM: m[col][row]
    m[0][0] = m00; m[1][0] = m01; m[2][0] = m02;
    m[0][1] = m10; m[1][1] = m11; m[2][1] = m12;
    m[0][2] = m20; m[1][2] = m21; m[2][2] = m22;
    return m;
}

// ---- packLTCMatrix tests ----

TEST(LUTPackingTests, IdentityMatrix) {
    // Identity: invM = identity, center diag = 1.0
    // Normalized: still identity
    // r = invM[0][0] = 1, g = invM[0][2] = 0, b = invM[2][0] = 0, a = invM[2][2] = 1
    ltc::Mat3 identity(1.0);
    ltc::PackedLTCMatrix packed = ltc::packLTCMatrix(identity);

    EXPECT_NEAR(packed.r, 1.0, 1e-10);
    EXPECT_NEAR(packed.g, 0.0, 1e-10);
    EXPECT_NEAR(packed.b, 0.0, 1e-10);
    EXPECT_NEAR(packed.a, 1.0, 1e-10);
}

TEST(LUTPackingTests, NonTrivialMatrix) {
    // M = [[2, 0, 0], [0, 3, 0], [1, 0, 1]] in row-major notation
    // As GLM column-major:
    //   col0 = (2, 0, 1), col1 = (0, 3, 0), col2 = (0, 0, 1)
    //   m[0][0]=2, m[0][1]=0, m[0][2]=1
    //   m[1][0]=0, m[1][1]=3, m[1][2]=0
    //   m[2][0]=0, m[2][1]=0, m[2][2]=1
    ltc::Mat3 M = makeMat3RowMajor(
        2, 0, 0,
        0, 3, 0,
        1, 0, 1
    );

    // The matrix rows in row-major: [2,0,0], [0,3,0], [1,0,1]
    // inverse of [[2,0,0],[0,3,0],[1,0,1]] = [[0.5,0,0],[0,1/3,0],[-0.5,0,1]]
    // Center element (1,1): inv[1][1] = 1/3
    // Normalize by 1/3:
    // inv / (1/3) = [[1.5,0,0],[0,1,0],[-1.5,0,3]]
    // In GLM [col][row] notation after normalization:
    //   r = invM[0][0] = 1.5   (col0, row0)
    //   g = invM[0][2] = -1.5  (col0, row2 = math element (row2, col0))
    //   b = invM[2][0] = 0     (col2, row0 = math element (row0, col2))
    //   a = invM[2][2] = 3     (col2, row2)

    // Re-verify using GLM column-major notation invM[col][row]:
    // Math inverse of [[2,0,0],[0,3,0],[1,0,1]] = [[0.5,0,0],[0,1/3,0],[-0.5,0,1]]
    // In GLM column-major:
    //   invM[0][0] = 0.5   (col0,row0) — math element (row0,col0) = 0.5
    //   invM[0][2] = -0.5  (col0,row2) — math element (row2,col0) = -0.5
    //   invM[1][1] = 1/3   (col1,row1) — center diagonal
    //   invM[2][2] = 1     (col2,row2)
    //   all others = 0
    // After /= (1/3):
    //   invM[0][0] = 1.5
    //   invM[0][2] = -1.5
    //   invM[1][1] = 1
    //   invM[2][0] = 0
    //   invM[2][2] = 3
    // packed: r=invM[0][0]=1.5, g=invM[0][2]=-1.5, b=invM[2][0]=0, a=invM[2][2]=3

    ltc::PackedLTCMatrix packed = ltc::packLTCMatrix(M);
    EXPECT_NEAR(packed.r,  1.5,  1e-9) << "r should be 1.5";
    EXPECT_NEAR(packed.g, -1.5,  1e-9) << "g should be -1.5";
    EXPECT_NEAR(packed.b,  0.0,  1e-9) << "b should be 0";
    EXPECT_NEAR(packed.a,  3.0,  1e-9) << "a should be 3";
}

TEST(LUTPackingTests, SingularMatrixThrows) {
    // All-zero matrix is singular
    ltc::Mat3 singular(0.0);
    EXPECT_THROW(ltc::packLTCMatrix(singular), std::runtime_error);
}

TEST(LUTPackingTests, ZeroCenterDiagonalThrows) {
    // Create a matrix whose inverse has zero center diagonal element
    // Tricky: we want inv[1][1] = 0
    // If M has a zero in position [1][1] after inversion...
    // Try: M = [[1,0,0],[0,0,1],[0,1,0]] (permutation matrix swapping row/col 1 and 2)
    // inv(M) = M (permutation matrices are self-inverse)
    // inv[col=1][row=1] = M[1][1] = 0 -> should throw
    ltc::Mat3 M = makeMat3RowMajor(
        1, 0, 0,
        0, 0, 1,
        0, 1, 0
    );
    EXPECT_THROW(ltc::packLTCMatrix(M), std::runtime_error);
}

TEST(LUTPackingTests, NegativeCoefficientsPreserved) {
    // A matrix where the packed coefficients should include negatives
    // Use a simple rotation-like matrix that produces negative packed values
    // M = [[1, 0, 0.5], [0, 1, 0], [0, 0, 1]] in row-major
    // inv: simple upper triangular
    // inv[0][0]=1, inv[2][0]=-0.5, inv[1][1]=1, inv[2][2]=1
    ltc::Mat3 M = makeMat3RowMajor(
        1, 0, 0.5,
        0, 1, 0,
        0, 0, 1
    );
    ltc::PackedLTCMatrix packed = ltc::packLTCMatrix(M);
    // inv: [[1,0,-0.5],[0,1,0],[0,0,1]]
    // denom = inv[1][1] = 1, normalized same
    // r=1, g=inv[0][2]=0, b=inv[2][0]=-0.5, a=1
    EXPECT_NEAR(packed.r,  1.0,  1e-9);
    EXPECT_NEAR(packed.g,  0.0,  1e-9);
    EXPECT_NEAR(packed.b, -0.5,  1e-9);
    EXPECT_NEAR(packed.a,  1.0,  1e-9);
}

// ---- computeSphericalCapCoefficient tests ----

TEST(SphericalCapTests, FullyGrazing) {
    // ndotv = 0 → 2*pi (full hemisphere)
    double val = ltc::computeSphericalCapCoefficient(0.0);
    EXPECT_NEAR(val, 2.0 * ltc::kPi, 1e-10);
}

TEST(SphericalCapTests, Perpendicular) {
    // ndotv = 1 → 0 (looking straight up, no cap)
    double val = ltc::computeSphericalCapCoefficient(1.0);
    EXPECT_NEAR(val, 0.0, 1e-10);
}

TEST(SphericalCapTests, HalfAngle) {
    // ndotv = 0.5 → 2*pi * 0.5 = pi
    double val = ltc::computeSphericalCapCoefficient(0.5);
    EXPECT_NEAR(val, ltc::kPi, 1e-10);
}

TEST(SphericalCapTests, ClampBelowZero) {
    // ndotv < 0 should clamp to ndotv=0 → 2*pi
    double val = ltc::computeSphericalCapCoefficient(-0.5);
    EXPECT_NEAR(val, 2.0 * ltc::kPi, 1e-10);
}

TEST(SphericalCapTests, ClampAboveOne) {
    // ndotv > 1 should clamp to ndotv=1 → 0
    double val = ltc::computeSphericalCapCoefficient(1.5);
    EXPECT_NEAR(val, 0.0, 1e-10);
}

} // namespace
