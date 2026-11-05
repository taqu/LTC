#include "ltc/FittingDiagnostics.h"
#include <cmath>

namespace ltc {

double computeMatrixConditionNumber(const Mat3& matrix, const Mat3& inverseMatrix)
{
    double mNorm  = 0.0;
    double miNorm = 0.0;
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            double mv  = static_cast<double>(matrix[col][row]);
            double miv = static_cast<double>(inverseMatrix[col][row]);
            if (!std::isfinite(mv) || !std::isfinite(miv)) return 0.0;
            mNorm  += mv  * mv;
            miNorm += miv * miv;
        }
    }
    return std::sqrt(mNorm) * std::sqrt(miNorm);
}

bool isMatrixNumericallyValid(const Mat3& matrix)
{
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            if (!std::isfinite(static_cast<double>(matrix[col][row])))
                return false;
        }
    }
    return true;
}

} // namespace ltc
