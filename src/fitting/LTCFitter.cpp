#include "ltc/Fitter.h"
#include "ltc/BRDFIntegration.h"
#include "ltc/ErrorFunction.h"
#include "ltc/NelderMead.h"
#include "ltc/LTC.h"
#include <algorithm>
#include <cmath>

namespace ltc {

namespace {

void buildLTCBasis(const Vec3& avgDir, Vec3& X, Vec3& Y, Vec3& Z)
{
    Z = avgDir;
    X = Vec3(Z.z, Scalar(0), -Z.x);
    Y = Vec3(Scalar(0), Scalar(1), Scalar(0));
}

bool applyParams3D(LTC& ltc, double m11, double m22, double m13, double minScale,
                   const Vec3& X, const Vec3& Y, const Vec3& Z)
{
    m11 = std::max(m11, minScale);
    m22 = std::max(m22, minScale);
    return ltc.setBasis(X, Y, Z) && ltc.setParameters(m11, m22, m13);
}

bool applyParams1D(LTC& ltc, double scale, double minScale,
                   const Vec3& X, const Vec3& Y, const Vec3& Z)
{
    scale = std::max(scale, minScale);
    return ltc.setBasis(X, Y, Z) && ltc.setParameters(scale, scale, 0.0);
}

} // anonymous namespace

// ---- Core fitting logic (shared by both public overloads) ----

FittingResult LTCFitter::fit(
    const IBRDF&                 brdf,
    const BRDFContext&           ctx,
    const BRDFIntegrationResult& integ,
    const FittingConfig&         config,
    const LTCInitialGuess&       initial) const
{
    FittingResult result;

    if (ctx.alpha <= Scalar(0) || !isFinite(ctx.view) || ctx.view.z <= Scalar(0)) {
        result.status = FitStatus::InvalidInput;
        return result;
    }

    result.magnitude             = integ.magnitude;
    result.fresnelCoefficient    = integ.fresnelCoefficient;
    result.hasFresnelCoefficient = integ.hasFresnelCoefficient;
    result.averageDirection      = integ.averageDirection;

    if (integ.magnitude < 1e-10) {
        result.status = FitStatus::ZeroEnergy;
        return result;
    }

    // ---- Build basis ----
    bool normalIncidence = (ctx.view.z >= Scalar(1) - Scalar(1e-6));

    Vec3 basisX, basisY, basisZ;
    if (normalIncidence || !integ.hasValidDirection) {
        basisX = Vec3(1, 0, 0);
        basisY = Vec3(0, 1, 0);
        basisZ = Vec3(0, 0, 1);
    } else {
        buildLTCBasis(integ.averageDirection, basisX, basisY, basisZ);
    }

    // ---- Setup optimizer and error function ----
    LTCErrorFunction errFn;
    NelderMead optimizer;

    LTC workLtc;
    workLtc.setMagnitude(integ.magnitude);

    if (normalIncidence) {
        // ---- 1-D optimization: single scale parameter ----
        double scale0 = initial.m11;

        applyParams1D(workLtc, scale0, config.minScale, basisX, basisY, basisZ);
        result.initialError = errFn.evaluate(brdf, ctx, workLtc, config);

        auto objective = [&](std::span<const double> p) -> double {
            if (!applyParams1D(workLtc, p[0], config.minScale, basisX, basisY, basisZ))
                return 1e30;
            return errFn.evaluate(brdf, ctx, workLtc, config);
        };

        double init0[1] = {scale0};
        auto opt = optimizer.minimize(
            objective,
            std::span<const double>(init0, 1),
            config.simplexStep,
            config.tolerance,
            config.maxIterations);

        double finalScale = std::max(opt.parameters[0], config.minScale);
        result.parameters.m11   = finalScale;
        result.parameters.m22   = finalScale;
        result.parameters.m13   = 0.0;
        result.parameters.basis = Mat3(basisX, basisY, basisZ);
        result.finalError       = opt.objectiveValue;
        result.iterations       = opt.iterations;
        result.evaluations      = opt.evaluations;
        result.status = (opt.status == OptimizationStatus::Converged)
                            ? FitStatus::Converged : FitStatus::MaxIterations;

    } else {
        // ---- 3-D optimization: (m11, m22, m13) ----
        double m11_0 = initial.m11;
        double m22_0 = initial.m22;
        double m13_0 = initial.m13;

        applyParams3D(workLtc, m11_0, m22_0, m13_0, config.minScale, basisX, basisY, basisZ);
        result.initialError = errFn.evaluate(brdf, ctx, workLtc, config);

        auto objective = [&](std::span<const double> p) -> double {
            if (!applyParams3D(workLtc, p[0], p[1], p[2], config.minScale, basisX, basisY, basisZ))
                return 1e30;
            return errFn.evaluate(brdf, ctx, workLtc, config);
        };

        double init3[3] = {m11_0, m22_0, m13_0};
        auto opt = optimizer.minimize(
            objective,
            std::span<const double>(init3, 3),
            config.simplexStep,
            config.tolerance,
            config.maxIterations);

        double m11f = std::max(opt.parameters[0], config.minScale);
        double m22f = std::max(opt.parameters[1], config.minScale);
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
    }

    return result;
}

// ---- Convenience overload: integrates the BRDF then delegates ----

FittingResult LTCFitter::fit(
    const IBRDF&           brdf,
    const BRDFContext&     ctx,
    const FittingConfig&   config,
    const LTCInitialGuess& initial) const
{
    if (ctx.alpha <= Scalar(0) || !isFinite(ctx.view) || ctx.view.z <= Scalar(0)) {
        FittingResult r;
        r.status = FitStatus::InvalidInput;
        return r;
    }

    BRDFIntegrator integrator;
    BRDFIntegrationResult integ = integrator.integrate(brdf, ctx, config);
    return fit(brdf, ctx, integ, config, initial);
}

} // namespace ltc
