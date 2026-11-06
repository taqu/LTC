#pragma once
#include "Math.h"

namespace ltc {

// Frobenius-norm condition estimate: κ_F(M) = ||M||_F × ||M⁻¹||_F
// Gives a conservative upper bound on the true 2-norm condition number.
// Returns 0 if any entry is non-finite.
double computeMatrixConditionNumber(const Mat3& matrix, const Mat3& inverseMatrix);

// Returns false when any entry of `matrix` is non-finite.
// Does not check the condition number; use computeMatrixConditionNumber for that.
bool isMatrixNumericallyValid(const Mat3& matrix);

} // namespace ltc
