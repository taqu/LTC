#include "MultiStartOptimizer.h"
#include "ltc/ErrorFunction.h"
#include "ltc/NelderMead.h"
#include <algorithm>
#include <cmath>

namespace ltc {
namespace detail {

namespace {

bool applyParams1D(LTC& ltc, double scale, double minScale,
                   const Vec3& X, const Vec3& Y, const Vec3& Z)
{
    scale = std::max(scale, minScale);
    return ltc.setBasis(X, Y, Z) && ltc.setParameters(
        Scalar(scale), Scalar(scale), Scalar(0));
}

bool applyParams3D(LTC& ltc, double m11, double m22, double m13, double minScale,
                   const Vec3& X, const Vec3& Y, const Vec3& Z)
{
    m11 = std::max(m11, minScale);
    m22 = std::max(m22, minScale);
    return ltc.setBasis(X, Y, Z) && ltc.setParameters(
        Scalar(m11), Scalar(m22), Scalar(m13));
}

} // anonymous namespace

CandidateResult runCandidateOptimization(
    const IBRDF&                brdf,
    const BRDFContext&          ctx,
    double                      magnitude,
    const Vec3&                 basisX,
    const Vec3&                 basisY,
    const Vec3&                 basisZ,
    bool                        normalIncidence,
    const FittingConfig&        stageConfig,
    const QualityFittingConfig& qcfg,
    const LTCInitialGuess&      initial)
{
    LTCErrorFunction errFn;
    NelderMead       optimizer;

    LTC workLtc;
    workLtc.setMagnitude(Scalar(magnitude));

    CandidateResult cr;

    // Build validation config: same settings but different sample resolution.
    FittingConfig valCfg  = stageConfig;
    valCfg.sampleResolution = qcfg.validationSampleResolution;

    FittingResult result;
    result.magnitude = magnitude;

    if (normalIncidence) {
        double scale0 = initial.m11;

        if (qcfg.logSpaceScales) {
            double s0 = std::log(std::max(scale0, stageConfig.minScale));

            applyParams1D(workLtc, scale0, stageConfig.minScale, basisX, basisY, basisZ);
            result.initialError = errFn.evaluate(brdf, ctx, workLtc, stageConfig);

            auto objective = [&](std::span<const double> p) -> double {
                double s  = std::clamp(p[0], -16.0, 16.0);
                double sc = std::exp(s);
                if (!applyParams1D(workLtc, sc, stageConfig.minScale, basisX, basisY, basisZ))
                    return 1e30;
                return errFn.evaluate(brdf, ctx, workLtc, stageConfig);
            };

            double init1[1] = {s0};
            auto opt = optimizer.minimize(
                objective, std::span<const double>(init1, 1),
                stageConfig.simplexStep, stageConfig.tolerance, stageConfig.maxIterations);

            double sf   = std::clamp(opt.parameters[0], -16.0, 16.0);
            double scf  = std::max(std::exp(sf), stageConfig.minScale);
            result.parameters.m11   = scf;
            result.parameters.m22   = scf;
            result.parameters.m13   = 0.0;
            result.parameters.basis = Mat3(basisX, basisY, basisZ);
            result.finalError       = opt.objectiveValue;
            result.iterations       = opt.iterations;
            result.evaluations      = opt.evaluations;
            result.status = (opt.status == OptimizationStatus::Converged)
                                ? FitStatus::Converged : FitStatus::MaxIterations;
        } else {
            applyParams1D(workLtc, scale0, stageConfig.minScale, basisX, basisY, basisZ);
            result.initialError = errFn.evaluate(brdf, ctx, workLtc, stageConfig);

            auto objective = [&](std::span<const double> p) -> double {
                if (!applyParams1D(workLtc, p[0], stageConfig.minScale, basisX, basisY, basisZ))
                    return 1e30;
                return errFn.evaluate(brdf, ctx, workLtc, stageConfig);
            };

            double init1[1] = {scale0};
            auto opt = optimizer.minimize(
                objective, std::span<const double>(init1, 1),
                stageConfig.simplexStep, stageConfig.tolerance, stageConfig.maxIterations);

            double scf = std::max(opt.parameters[0], stageConfig.minScale);
            result.parameters.m11   = scf;
            result.parameters.m22   = scf;
            result.parameters.m13   = 0.0;
            result.parameters.basis = Mat3(basisX, basisY, basisZ);
            result.finalError       = opt.objectiveValue;
            result.iterations       = opt.iterations;
            result.evaluations      = opt.evaluations;
            result.status = (opt.status == OptimizationStatus::Converged)
                                ? FitStatus::Converged : FitStatus::MaxIterations;
        }
    } else {
        double m11_0 = initial.m11;
        double m22_0 = initial.m22;
        double m13_0 = initial.m13;

        if (qcfg.logSpaceScales) {
            double s1_0 = std::log(std::max(m11_0, stageConfig.minScale));
            double s2_0 = std::log(std::max(m22_0, stageConfig.minScale));

            applyParams3D(workLtc, m11_0, m22_0, m13_0, stageConfig.minScale,
                          basisX, basisY, basisZ);
            result.initialError = errFn.evaluate(brdf, ctx, workLtc, stageConfig);

            auto objective = [&](std::span<const double> p) -> double {
                double s1  = std::clamp(p[0], -16.0, 16.0);
                double s2  = std::clamp(p[1], -16.0, 16.0);
                double m11 = std::exp(s1);
                double m22 = std::exp(s2);
                double m13 = p[2];
                if (!applyParams3D(workLtc, m11, m22, m13, stageConfig.minScale,
                                   basisX, basisY, basisZ))
                    return 1e30;
                return errFn.evaluate(brdf, ctx, workLtc, stageConfig);
            };

            double init3[3] = {s1_0, s2_0, m13_0};
            auto opt = optimizer.minimize(
                objective, std::span<const double>(init3, 3),
                stageConfig.simplexStep, stageConfig.tolerance, stageConfig.maxIterations);

            double s1f  = std::clamp(opt.parameters[0], -16.0, 16.0);
            double s2f  = std::clamp(opt.parameters[1], -16.0, 16.0);
            double m11f = std::max(std::exp(s1f), stageConfig.minScale);
            double m22f = std::max(std::exp(s2f), stageConfig.minScale);
            double m13f = opt.parameters[2];
            result.parameters.m11   = m11f;
            result.parameters.m22   = m22f;
            result.parameters.m13   = m13f;
            result.parameters.basis = Mat3(basisX, basisY, basisZ);
            result.finalError       = opt.objectiveValue;
            result.iterations       = opt.iterations;
            result.evaluations      = opt.evaluations;
            result.status = (opt.status == OptimizationStatus::Converged)
                                ? FitStatus::Converged : FitStatus::MaxIterations;
        } else {
            applyParams3D(workLtc, m11_0, m22_0, m13_0, stageConfig.minScale,
                          basisX, basisY, basisZ);
            result.initialError = errFn.evaluate(brdf, ctx, workLtc, stageConfig);

            auto objective = [&](std::span<const double> p) -> double {
                if (!applyParams3D(workLtc, p[0], p[1], p[2], stageConfig.minScale,
                                   basisX, basisY, basisZ))
                    return 1e30;
                return errFn.evaluate(brdf, ctx, workLtc, stageConfig);
            };

            double init3[3] = {m11_0, m22_0, m13_0};
            auto opt = optimizer.minimize(
                objective, std::span<const double>(init3, 3),
                stageConfig.simplexStep, stageConfig.tolerance, stageConfig.maxIterations);

            result.parameters.m11   = std::max(opt.parameters[0], stageConfig.minScale);
            result.parameters.m22   = std::max(opt.parameters[1], stageConfig.minScale);
            result.parameters.m13   = opt.parameters[2];
            result.parameters.basis = Mat3(basisX, basisY, basisZ);
            result.finalError       = opt.objectiveValue;
            result.iterations       = opt.iterations;
            result.evaluations      = opt.evaluations;
            result.status = (opt.status == OptimizationStatus::Converged)
                                ? FitStatus::Converged : FitStatus::MaxIterations;
        }
    }

    // Compute independent validation objective.
    LTC validLtc;
    validLtc.setMagnitude(Scalar(magnitude));
    Vec3 pbX(result.parameters.basis[0]),
         pbY(result.parameters.basis[1]),
         pbZ(result.parameters.basis[2]);
    if (validLtc.setBasis(pbX, pbY, pbZ) &&
        validLtc.setParameters(
            Scalar(result.parameters.m11),
            Scalar(result.parameters.m22),
            Scalar(result.parameters.m13)))
    {
        cr.validationObjective = errFn.evaluate(brdf, ctx, validLtc, valCfg);
        cr.valid               = std::isfinite(cr.validationObjective);
    }

    cr.result = result;
    return cr;
}

} // namespace detail
} // namespace ltc
