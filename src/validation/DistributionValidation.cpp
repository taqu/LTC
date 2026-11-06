#include "ltc/DistributionValidation.h"
#include "ltc/LTC.h"
#include <algorithm>
#include <cmath>

namespace ltc {

DistributionErrorMetrics validateDistribution(
    const IBRDF& brdf,
    const LUTCell& cell,
    const LUTParameters& params,
    const DistributionValidationConfig& config)
{
    // Build LTC from cell parameters
    LTC ltc;
    ltc.setParameters(
        Scalar(cell.parameters.m11),
        Scalar(cell.parameters.m22),
        Scalar(cell.parameters.m13));

    // Set the basis from cell.parameters.basis (columns are X, Y, Z)
    // GLM: basis[0] = first column = X, basis[1] = Y, basis[2] = Z
    const Mat3& basis = cell.parameters.basis;
    ltc.setBasis(basis[0], basis[1], basis[2]);

    ltc.setMagnitude(Scalar(cell.magnitude));

    BRDFContext ctx{params.viewDirection, Scalar(params.alpha)};

    uint32_t N = config.sampleCount;
    uint32_t sqrtN = (uint32_t)std::sqrt((double)N);
    if (sqrtN < 1) sqrtN = 1;

    double sumL1 = 0, sumL2 = 0, sumL3 = 0;
    double sumBRDF = 0, sumLTC = 0;
    double maxErr = 0;
    Vec3 maxErrDir{0, 0, 1};
    uint32_t count = 0;

    // Uniform hemisphere stratified sampling
    // This differs from the cosine-weighted fitting grid
    for (uint32_t j = 0; j < sqrtN; ++j) {
        for (uint32_t i = 0; i < sqrtN; ++i) {
            // Uniform hemisphere: cosTheta uniformly in [0, 1]
            double cosTheta = (j + 0.5) / sqrtN;
            double sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
            double phi = kTwoPi * (i + 0.5) / sqrtN;

            Vec3 L(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);

            // BRDF returns cosine-weighted value: f_r * ndotl
            double f_B = brdf.evaluate(ctx, L).value;
            double f_L = ltc.evaluate(L);

            double diff = std::abs(f_B - f_L);
            sumL1 += diff;
            sumL2 += diff * diff;
            sumL3 += diff * diff * diff;
            sumBRDF += f_B;
            sumLTC += f_L;

            if (diff > maxErr) {
                maxErr = diff;
                maxErrDir = L;
            }
            ++count;
        }
    }

    if (count == 0) {
        return DistributionErrorMetrics{};
    }

    // Solid angle per sample for uniform hemisphere: 2*pi / count
    double dOmega = kTwoPi / static_cast<double>(count);

    DistributionErrorMetrics m;
    m.l1Error = sumL1 * dOmega;
    m.l2Error = std::sqrt(std::max(0.0, sumL2 * dOmega));
    m.cubicError = sumL3 * dOmega;
    m.energyError = std::abs((sumBRDF - sumLTC) * dOmega);
    m.maxAbsError = maxErr;
    m.maxErrorDirection = maxErrDir;
    m.sampleCount = count;
    return m;
}

} // namespace ltc
