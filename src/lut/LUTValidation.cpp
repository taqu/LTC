#include "ltc/LUTValidation.h"
#include "ltc/LTC.h"
#include <cmath>
#include <limits>

namespace ltc {

ValidationReport LUTValidator::validate(const LUTData& lut) const
{
    ValidationReport r;
    r.totalCells = lut.width() * lut.height();

    double sumErr  = 0.0;
    double sumErr2 = 0.0;
    r.minFinalError = std::numeric_limits<double>::max();
    r.maxFinalError = -std::numeric_limits<double>::max();

    for (uint32_t t = 0; t < lut.height(); ++t) {
        for (uint32_t a = 0; a < lut.width(); ++a) {
            const LUTCell& cell = lut.at(a, t);

            if (!cell.completed) continue;
            ++r.completedCells;

            switch (cell.status) {
                case FitStatus::Converged:     ++r.convergedCells; break;
                case FitStatus::MaxIterations: ++r.maxIterCells;   break;
                default:                       ++r.failedCells;    break;
            }

            // Finite coefficient check
            const auto& p = cell.parameters;
            if (!std::isfinite(p.m11) || !std::isfinite(p.m22) || !std::isfinite(p.m13) ||
                !std::isfinite(cell.magnitude) || !std::isfinite(cell.fresnelCoefficient)) {
                r.allFinite = false;
                ++r.nonFiniteCount;
            }

            // Invertibility check
            if (cell.parameters.m11 <= 0.0 || cell.parameters.m22 <= 0.0) {
                r.allInvertible = false;
                ++r.nonInvertibleCount;
            }

            // Positive magnitude check
            if (cell.magnitude <= 0.0) {
                r.allPositiveMagnitude = false;
                ++r.nonPositiveMagnitudeCount;
            }

            // Error statistics
            if (std::isfinite(cell.finalError)) {
                ++r.finiteErrorCount;
                sumErr  += cell.finalError;
                sumErr2 += cell.finalError * cell.finalError;
                if (cell.finalError < r.minFinalError) r.minFinalError = cell.finalError;
                if (cell.finalError > r.maxFinalError) r.maxFinalError = cell.finalError;
            }
        }
    }

    r.isComplete = (r.completedCells == r.totalCells);

    if (r.finiteErrorCount == 0) {
        r.minFinalError = 0.0;
        r.maxFinalError = 0.0;
    }

    if (r.finiteErrorCount > 0) {
        r.meanFinalError = sumErr  / r.finiteErrorCount;
        r.rmsFinalError  = std::sqrt(sumErr2 / r.finiteErrorCount);
    }

    // Continuity: scan horizontal (roughness) and vertical (view) neighbors
    for (uint32_t t = 0; t < lut.height(); ++t) {
        for (uint32_t a = 0; a < lut.width(); ++a) {
            const LUTCell& c = lut.at(a, t);
            if (!c.completed) continue;

            // Horizontal neighbor (roughness)
            if (a + 1 < lut.width()) {
                const LUTCell& n = lut.at(a + 1, t);
                if (n.completed) {
                    double dm11 = std::abs(c.parameters.m11 - n.parameters.m11);
                    double dm22 = std::abs(c.parameters.m22 - n.parameters.m22);
                    double dm13 = std::abs(c.parameters.m13 - n.parameters.m13);
                    double dMag = std::abs(c.magnitude - n.magnitude);
                    if (dm11 > r.maxM11Jump) r.maxM11Jump = dm11;
                    if (dm22 > r.maxM22Jump) r.maxM22Jump = dm22;
                    if (dm13 > r.maxM13Jump) r.maxM13Jump = dm13;
                    if (dMag > r.maxMagnitudeJump) r.maxMagnitudeJump = dMag;
                }
            }

            // Vertical neighbor (view angle)
            if (t + 1 < lut.height()) {
                const LUTCell& n = lut.at(a, t + 1);
                if (n.completed) {
                    double dm11 = std::abs(c.parameters.m11 - n.parameters.m11);
                    double dm22 = std::abs(c.parameters.m22 - n.parameters.m22);
                    double dm13 = std::abs(c.parameters.m13 - n.parameters.m13);
                    double dMag = std::abs(c.magnitude - n.magnitude);
                    if (dm11 > r.maxM11Jump) r.maxM11Jump = dm11;
                    if (dm22 > r.maxM22Jump) r.maxM22Jump = dm22;
                    if (dm13 > r.maxM13Jump) r.maxM13Jump = dm13;
                    if (dMag > r.maxMagnitudeJump) r.maxMagnitudeJump = dMag;
                }
            }
        }
    }

    return r;
}

} // namespace ltc
