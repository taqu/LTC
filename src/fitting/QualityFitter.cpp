#include "ltc/QualityFitting.h"
#include "MultiStartOptimizer.h"
#include "ltc/AdaptiveSampling.h"
#include "ltc/FittingDiagnostics.h"
#include "ltc/InitialGuessStrategy.h"
#include "ltc/LTC.h"
#include <algorithm>

namespace ltc {

QualityFittingResult QualityFitter::fit(
    const IBRDF&                 brdf,
    const BRDFContext&           ctx,
    const BRDFIntegrationResult& integ,
    const FittingConfig&         baseConfig,
    const QualityFittingConfig&  qcfg,
    const LTCInitialGuess&       initial) const
{
    QualityFittingResult qr;
    qr.diagnostics.status = QualityFitStatus::NumericalFailure;

    // Input validation (mirrors LTCFitter).
    if (ctx.alpha <= Scalar(0) || !isFinite(ctx.view) || ctx.view.z <= Scalar(0)) {
        qr.base.status = FitStatus::InvalidInput;
        return qr;
    }

    qr.base.magnitude             = integ.magnitude;
    qr.base.fresnelCoefficient    = integ.fresnelCoefficient;
    qr.base.hasFresnelCoefficient = integ.hasFresnelCoefficient;
    qr.base.averageDirection      = integ.averageDirection;

    if (integ.magnitude < 1e-10) {
        qr.base.status       = FitStatus::ZeroEnergy;
        qr.diagnostics.status= QualityFitStatus::NumericalFailure;
        return qr;
    }

    // Build orthonormal basis (reference-compatible).
    bool normalIncidence = (ctx.view.z >= Scalar(1) - Scalar(1e-6));
    Vec3 bX, bY, bZ;
    if (normalIncidence || !integ.hasValidDirection) {
        bX = Vec3(1, 0, 0);
        bY = Vec3(0, 1, 0);
        bZ = Vec3(0, 0, 1);
    } else {
        bZ = integ.averageDirection;
        bX = Vec3(bZ.z, Scalar(0), -bZ.x);
        bY = Vec3(Scalar(0), Scalar(1), Scalar(0));
    }

    // Build stage config: use quality tolerances over the base config.
    FittingConfig stageCfgBase  = baseConfig;
    stageCfgBase.maxIterations  = qcfg.maximumIterations;
    stageCfgBase.tolerance      = qcfg.convergenceTolerance;
    stageCfgBase.minScale       = qcfg.minimumScale;

    // Sampling schedule.
    AdaptiveSamplingSchedule schedule(qcfg.initialSampleResolution,
                                      qcfg.maximumSampleResolution);
    const auto& stages = schedule.stages();

    // Initial candidate list.
    InitialGuessStrategy strategy;
    uint32_t maxCandidates = qcfg.multiStart ? (qcfg.maximumRestarts + 1u) : 1u;
    auto candidates = strategy.generateCandidates(initial, nullptr, maxCandidates);

    // Best-result tracking.
    detail::CandidateResult bestCR;
    bestCR.validationObjective = 1e30;
    bool    hasBest             = false;
    uint64_t totalEvals         = 0;
    uint32_t restartsDone       = 0;
    QualityFitStatus finalStatus= QualityFitStatus::MaximumSamplingResolution;
    bool     budgetExhausted    = false;

    for (size_t stageIdx = 0; stageIdx < stages.size(); ++stageIdx) {
        if (!qcfg.adaptiveSampling && stageIdx > 0) break;

        FittingConfig stageCfg          = stageCfgBase;
        stageCfg.sampleResolution       = stages[stageIdx].resolution;

        double prevBestValidation = bestCR.validationObjective;

        for (const auto& cand : candidates) {
            if (totalEvals >= qcfg.maximumTotalEvaluations) {
                finalStatus      = QualityFitStatus::MaximumEvaluations;
                budgetExhausted  = true;
                break;
            }

            detail::CandidateResult cr = detail::runCandidateOptimization(
                brdf, ctx, integ.magnitude, bX, bY, bZ,
                normalIncidence, stageCfg, qcfg, cand.guess);

            totalEvals += cr.result.evaluations;
            // Count validation sample evaluations.
            uint64_t valSamples = static_cast<uint64_t>(qcfg.validationSampleResolution)
                                * qcfg.validationSampleResolution * 2u;
            totalEvals += valSamples;

            if (cr.valid && cr.validationObjective < bestCR.validationObjective) {
                bestCR = cr;
                hasBest = true;
                finalStatus = (cr.result.status == FitStatus::Converged)
                                  ? QualityFitStatus::Converged
                                  : QualityFitStatus::MaximumIterations;
            }
            ++restartsDone;
        }

        if (budgetExhausted) break;

        // Stagnation check: if the stage improved little, stop early.
        if (stageIdx > 0 && hasBest) {
            double improvement = prevBestValidation - bestCR.validationObjective;
            if (improvement < qcfg.stagnationThreshold) {
                finalStatus = QualityFitStatus::Stagnated;
                break;
            }
        }

        // For subsequent stages, seed from best result found so far.
        if (hasBest && qcfg.multiStart && stageIdx + 1 < stages.size()) {
            LTCInitialGuess bestGuess{
                bestCR.result.parameters.m11,
                bestCR.result.parameters.m22,
                bestCR.result.parameters.m13};
            candidates = strategy.generateCandidates(initial, &bestGuess, maxCandidates);
        }
    }

    if (!hasBest) {
        qr.base.status       = FitStatus::NumericalFailure;
        qr.diagnostics.status= QualityFitStatus::NumericalFailure;
        return qr;
    }

    qr.base = bestCR.result;

    // Diagnostics.
    qr.diagnostics.initialObjective      = bestCR.result.initialError;
    qr.diagnostics.finalObjective        = bestCR.result.finalError;
    qr.diagnostics.validationObjective   = bestCR.validationObjective;
    qr.diagnostics.totalIterations       = bestCR.result.iterations;
    qr.diagnostics.totalEvaluations      = totalEvals;
    qr.diagnostics.restartCount          = restartsDone > 0 ? restartsDone - 1 : 0;
    qr.diagnostics.finalSampleResolution = qcfg.adaptiveSampling
                                             ? stages.back().resolution
                                             : stages.front().resolution;
    qr.diagnostics.minScale = std::min(bestCR.result.parameters.m11,
                                       bestCR.result.parameters.m22);
    qr.diagnostics.maxScale = std::max(bestCR.result.parameters.m11,
                                       bestCR.result.parameters.m22);
    qr.diagnostics.status   = finalStatus;

    // Matrix condition number.
    LTC finalLtc;
    finalLtc.setMagnitude(Scalar(integ.magnitude));
    Vec3 fbX(bestCR.result.parameters.basis[0]),
         fbY(bestCR.result.parameters.basis[1]),
         fbZ(bestCR.result.parameters.basis[2]);
    if (finalLtc.setBasis(fbX, fbY, fbZ) &&
        finalLtc.setParameters(
            Scalar(bestCR.result.parameters.m11),
            Scalar(bestCR.result.parameters.m22),
            Scalar(bestCR.result.parameters.m13)))
    {
        qr.diagnostics.matrixConditionNumber =
            computeMatrixConditionNumber(finalLtc.matrix(), finalLtc.inverseMatrix());
    }

    return qr;
}

} // namespace ltc
