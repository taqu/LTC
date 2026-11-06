#include "ltc/LUTRefinement.h"
#include "ltc/BRDFIntegration.h"
#include "ltc/ErrorFunction.h"
#include "ltc/LTC.h"
#include "ltc/ParameterGrid.h"
#include "ltc/QualityFitting.h"
#include <algorithm>
#include <cmath>

namespace ltc {

namespace {

// Compute the MIS validation objective for a completed LUTCell.
// Returns 1e30 if the cell or LTC is invalid.
double cellValidationError(
    const LUTCell&       cell,
    const IBRDF&         brdf,
    const BRDFContext&   ctx,
    const FittingConfig& valCfg)
{
    if (!cell.completed) return 1e30;
    if (cell.magnitude < 1e-10) return 1e30;

    LTC ltc;
    ltc.setMagnitude(Scalar(cell.magnitude));
    Vec3 bX(cell.parameters.basis[0]),
         bY(cell.parameters.basis[1]),
         bZ(cell.parameters.basis[2]);
    if (!ltc.setBasis(bX, bY, bZ)) return 1e30;
    if (!ltc.setParameters(Scalar(cell.parameters.m11),
                            Scalar(cell.parameters.m22),
                            Scalar(cell.parameters.m13)))
        return 1e30;

    LTCErrorFunction errFn;
    double val = errFn.evaluate(brdf, ctx, ltc, valCfg);
    return std::isfinite(val) ? val : 1e30;
}

LUTCell cellFromResult(const FittingResult& r)
{
    LUTCell cell;
    cell.parameters          = r.parameters;
    cell.magnitude           = r.magnitude;
    cell.fresnelCoefficient  = r.fresnelCoefficient;
    cell.hasFresnelCoefficient = r.hasFresnelCoefficient;
    cell.initialError        = r.initialError;
    cell.finalError          = r.finalError;
    cell.iterations          = r.iterations;
    cell.evaluations         = r.evaluations;
    cell.status              = r.status;
    cell.completed           = true;

    LTC ltc;
    const auto& p = r.parameters;
    Vec3 bX(p.basis[0]), bY(p.basis[1]), bZ(p.basis[2]);
    if (ltc.setBasis(bX, bY, bZ) && ltc.setParameters(
            Scalar(p.m11), Scalar(p.m22), Scalar(p.m13))) {
        cell.matrix        = ltc.matrix();
        cell.inverseMatrix = ltc.inverseMatrix();
    }
    return cell;
}

} // anonymous namespace

RefinementResult LUTRefiner::refine(
    LUTData&                    lut,
    const IBRDF&                brdf,
    const FittingConfig&        baseConfig,
    const QualityFittingConfig& qualityConfig,
    const RefinementCriteria&   criteria,
    GenerationProgress*         progress) const
{
    RefinementResult result;

    RefinementPolicy policy(criteria);
    auto candidates = policy.selectCandidates(lut);
    result.candidateCount = static_cast<uint32_t>(candidates.size());

    if (candidates.empty()) return result;

    if (progress) {
        progress->totalCells     = result.candidateCount;
        progress->completedCells.store(0);
    }

    ParameterGrid grid(lut.width(), lut.height());
    BRDFIntegrator integrator;
    QualityFitter  qFitter;

    // Validation config: use quality sample resolution for independent comparison.
    FittingConfig valCfg          = baseConfig;
    valCfg.sampleResolution       = qualityConfig.validationSampleResolution;

    double totalImprovement = 0.0;

    for (const auto& cand : candidates) {
        const LUTCell& original = lut.at(cand.roughnessIndex, cand.viewIndex);

        LUTCoordinates coords{cand.roughnessIndex, cand.viewIndex};
        LUTParameters  params = grid.evaluate(coords);
        BRDFContext     ctx{params.viewDirection,
                            Scalar(brdf.contextAlpha(params.perceptualRoughness))};

        // Re-integrate BRDF (not cached in LUTCell).
        BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, baseConfig);

        LTCInitialGuess guess{
            original.parameters.m11,
            original.parameters.m22,
            original.parameters.m13};

        QualityFittingResult qr = qFitter.fit(brdf, ctx, integ, baseConfig, qualityConfig, guess);
        if (qr.base.status == FitStatus::InvalidInput ||
            qr.base.status == FitStatus::ZeroEnergy   ||
            qr.base.status == FitStatus::NumericalFailure)
        {
            ++result.rejectedCount;
            if (progress) progress->completedCells.fetch_add(1);
            continue;
        }

        LUTCell refined = cellFromResult(qr.base);

        double origValErr = cellValidationError(original, brdf, ctx, valCfg);
        double refValErr  = cellValidationError(refined,  brdf, ctx, valCfg);

        if (policy.shouldAccept(original, refined, origValErr, refValErr)) {
            lut.at(cand.roughnessIndex, cand.viewIndex) = refined;
            ++result.refinedCount;
            double improvement = (origValErr - refValErr) /
                                  std::max(origValErr, 1e-15);
            totalImprovement += improvement;
        } else {
            ++result.rejectedCount;
        }

        if (progress) {
            progress->completedCells.fetch_add(1);
            if (progress->onCellComplete) progress->onCellComplete();
        }
    }

    if (result.refinedCount > 0)
        result.meanImprovement = totalImprovement / result.refinedCount;

    return result;
}

} // namespace ltc
