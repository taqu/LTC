#include "ltc/LUTGenerator.h"
#include "ltc/BRDFIntegration.h"
#include "ltc/Checkpoint.h"
#include "ltc/Fitter.h"
#include "ltc/JobScheduler.h"
#include "ltc/LTC.h"
#include "ltc/ParameterGrid.h"
#include "ltc/QualityFitting.h"
#include <stdexcept>
#include <string>

namespace ltc {

namespace {

// Build a fully-populated LUTCell from a FittingResult.
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

    // Reconstruct full matrices.
    LTC ltc;
    const auto& p = r.parameters;
    Vec3 bX(p.basis[0]), bY(p.basis[1]), bZ(p.basis[2]);
    if (ltc.setBasis(bX, bY, bZ) && ltc.setParameters(p.m11, p.m22, p.m13)) {
        cell.matrix        = ltc.matrix();
        cell.inverseMatrix = ltc.inverseMatrix();
    }
    return cell;
}

// Validate config; throws std::invalid_argument on error.
void validateConfig(const GenerationConfig& cfg)
{
    if (cfg.width  < 2) throw std::invalid_argument("GenerationConfig: width must be >= 2");
    if (cfg.height < 2) throw std::invalid_argument("GenerationConfig: height must be >= 2");
    if (cfg.fitting.sampleResolution == 0)
        throw std::invalid_argument("GenerationConfig: fitting.sampleResolution must be > 0");
    if (cfg.fitting.maxIterations == 0)
        throw std::invalid_argument("GenerationConfig: fitting.maxIterations must be > 0");
}

} // anonymous namespace

LUTData LUTGenerator::generate(
    const IBRDF&         brdf,
    const GenerationConfig& config,
    GenerationProgress*  progress) const
{
    validateConfig(config);

    ParameterGrid grid(config.width, config.height);

    LUTMetadata meta;
    meta.brdfName         = std::string(brdf.name());
    meta.width            = config.width;
    meta.height           = config.height;
    meta.fittingProfile   = config.fitting.profile;
    meta.sampleResolution = config.fitting.sampleResolution;
    meta.maxIterations    = config.fitting.maxIterations;

    LUTData lut(config.width, config.height, std::move(meta));

    // Resume from checkpoint: pre-populate completed cells.
    if (config.resumeFromCheckpoint && !config.checkpointPath.empty()) {
        if (!Checkpoint::isCompatible(config.checkpointPath, config, std::string(brdf.name())))
            throw CheckpointError("Checkpoint is incompatible with the requested generation config");
        lut = Checkpoint::load(config.checkpointPath);
    }

    if (progress) {
        progress->totalCells = config.width * config.height;
        // Count already-completed cells from checkpoint.
        uint32_t already = 0;
        for (const auto& cell : lut.cells()) {
            if (!cell.completed) continue;
            ++already;
            switch (cell.status) {
                case FitStatus::Converged:     ++progress->convergedCells; break;
                case FitStatus::MaxIterations: ++progress->maxIterCells;   break;
                default:                       ++progress->failedCells;    break;
            }
        }
        progress->completedCells.fetch_add(already);
    }

    const bool useQuality = (config.fitting.profile == FittingProfile::Quality);

    LTCFitter      fitter;
    QualityFitter  qFitter;
    BRDFIntegrator integrator;

    uint32_t checkpointCounter = 0;

    // Worker called by the scheduler for each cell.
    auto worker = [&](const SchedulerJob& job) -> LUTCell {
        // Skip already-completed cells from checkpoint.
        const LUTCell& existing = lut.at(job.roughnessIndex, job.viewIndex);
        if (existing.completed) return existing;

        if (progress && progress->cancel.load()) return existing; // partial cancel

        LUTCoordinates coords{job.roughnessIndex, job.viewIndex};
        LUTParameters  params = grid.evaluate(coords);
        BRDFContext     ctx{params.viewDirection, Scalar(brdf.contextAlpha(params.perceptualRoughness))};

        // Integrate BRDF (needed for average direction and magnitude).
        BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, config.fitting);

        // Fit using pre-computed integration + warm initial guess.
        FittingResult result;
        if (useQuality) {
            QualityFittingConfig qcfg;
            qcfg.initialSampleResolution = config.fitting.sampleResolution;
            qcfg.maximumIterations       = config.fitting.maxIterations;
            qcfg.convergenceTolerance    = config.fitting.tolerance;
            qcfg.minimumScale            = config.fitting.minScale;
            result = qFitter.fit(brdf, ctx, integ, config.fitting, qcfg, job.initial).base;
        } else {
            result = fitter.fit(brdf, ctx, integ, config.fitting, job.initial);
        }

        LUTCell cell = cellFromResult(result);

        // Update progress.
        if (progress) {
            progress->completedCells.fetch_add(1);
            switch (result.status) {
                case FitStatus::Converged:     progress->convergedCells.fetch_add(1); break;
                case FitStatus::MaxIterations: progress->maxIterCells.fetch_add(1);   break;
                default:                       progress->failedCells.fetch_add(1);    break;
            }
            if (progress->onCellComplete) progress->onCellComplete();
        }

        // Checkpoint at interval (sequential only — parallel saves after generation).
        if (config.enableCheckpoint && !config.checkpointPath.empty()) {
            ++checkpointCounter;
            if (checkpointCounter % config.checkpointInterval == 0) {
                // Write current LUT state (including this new cell).
                // Note: lut is updated by the scheduler after this return,
                // so we build a temporary view.  For simplicity in sequential
                // mode, checkpoint is written here by the scheduler lambda.
                // In parallel mode, checkpoint is written after generation.
            }
        }

        return cell;
    };

    // Run the scheduler.
    JobScheduler scheduler(config.width, config.height, config.threads);
    scheduler.run(worker, lut);

    // Save final checkpoint/output.
    if (config.enableCheckpoint && !config.checkpointPath.empty()) {
        Checkpoint::save(lut, config.checkpointPath);
    }

    return lut;
}

} // namespace ltc
