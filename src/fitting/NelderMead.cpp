#include "ltc/NelderMead.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace ltc {

namespace {

// Sort the simplex in-place (ascending order of objective values).
void sortSimplex(std::vector<std::vector<double>>& verts, std::vector<double>& fval)
{
    int m = static_cast<int>(verts.size());
    // Simple insertion sort (m is at most 4 for our use cases).
    for (int i = 1; i < m; ++i) {
        for (int j = i; j > 0 && fval[j] < fval[j - 1]; --j) {
            std::swap(fval[j], fval[j - 1]);
            std::swap(verts[j], verts[j - 1]);
        }
    }
}

// Compute centroid of vertices [0 .. n-1] (all except worst/last).
std::vector<double> centroid(const std::vector<std::vector<double>>& verts, int n)
{
    int dim = static_cast<int>(verts[0].size());
    std::vector<double> c(dim, 0.0);
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < dim; ++k)
            c[k] += verts[i][k] / n;
    return c;
}

} // anonymous namespace

OptimizationResult NelderMead::minimize(
    const Objective&          objective,
    std::span<const double>   initial,
    double                    initialStep,
    double                    tolerance,
    uint32_t                  maxIterations) const
{
    int n = static_cast<int>(initial.size());
    int m = n + 1;

    // ---- Initialize simplex ----
    std::vector<std::vector<double>> verts(m, std::vector<double>(n));
    std::vector<double> fval(m);

    for (int k = 0; k < n; ++k)
        verts[0][k] = initial[k];
    fval[0] = objective(std::span<const double>(verts[0]));
    uint64_t neval = 1;

    if (!std::isfinite(fval[0])) {
        return {std::vector<double>(initial.begin(), initial.end()),
                fval[0], 0, neval, OptimizationStatus::InvalidInitialPoint};
    }

    for (int i = 1; i < m; ++i) {
        verts[i] = verts[0];
        verts[i][i - 1] += initialStep;
        fval[i] = objective(std::span<const double>(verts[i]));
        ++neval;
        if (!std::isfinite(fval[i]))
            fval[i] = 1e30; // Treat non-finite as a large penalty.
    }

    sortSimplex(verts, fval);

    // ---- NM constants ----
    constexpr double kAlpha = 1.0; // reflection
    constexpr double kGamma = 2.0; // expansion
    constexpr double kRho   = 0.5; // contraction
    constexpr double kSigma = 0.5; // shrink
    (void)kAlpha; // reflection factor baked into formula: x_r = 2*c - worst
    (void)kGamma; // expansion factor baked into formula: x_e = 3*c - 2*worst

    for (uint32_t iter = 0; iter < maxIterations; ++iter) {
        // Sort at start of each iteration.
        sortSimplex(verts, fval);

        // Best = verts[0], Worst = verts[n].
        double f_best  = fval[0];
        double f_worst = fval[n];
        double f_2nd   = fval[n - 1]; // second worst

        // Convergence check.
        if (std::abs(f_worst - f_best) < tolerance) {
            return {verts[0], f_best, iter, neval, OptimizationStatus::Converged};
        }

        // Compute centroid of all but worst.
        std::vector<double> c = centroid(verts, n);

        // Reflect: x_r = 2*c - x_worst
        std::vector<double> x_r(n);
        for (int k = 0; k < n; ++k)
            x_r[k] = 2.0 * c[k] - verts[n][k];
        double f_r = objective(std::span<const double>(x_r));
        ++neval;
        if (!std::isfinite(f_r)) f_r = 1e30;

        if (f_r < f_best) {
            // Try expansion: x_e = 3*c - 2*x_worst
            std::vector<double> x_e(n);
            for (int k = 0; k < n; ++k)
                x_e[k] = 3.0 * c[k] - 2.0 * verts[n][k];
            double f_e = objective(std::span<const double>(x_e));
            ++neval;
            if (!std::isfinite(f_e)) f_e = 1e30;

            if (f_e < f_r) {
                verts[n] = std::move(x_e);
                fval[n]  = f_e;
            } else {
                verts[n] = std::move(x_r);
                fval[n]  = f_r;
            }
        } else if (f_r <= f_2nd) {
            // Accept reflection.
            verts[n] = std::move(x_r);
            fval[n]  = f_r;
        } else {
            // Contraction.
            bool accepted = false;
            if (f_r < f_worst) {
                // Outside contraction: c + rho*(x_r - c)
                std::vector<double> x_c(n);
                for (int k = 0; k < n; ++k)
                    x_c[k] = c[k] + kRho * (x_r[k] - c[k]);
                double f_c = objective(std::span<const double>(x_c));
                ++neval;
                if (!std::isfinite(f_c)) f_c = 1e30;
                if (f_c <= f_r) {
                    verts[n] = std::move(x_c);
                    fval[n]  = f_c;
                    accepted = true;
                }
            } else {
                // Inside contraction: c + rho*(x_worst - c)
                std::vector<double> x_c(n);
                for (int k = 0; k < n; ++k)
                    x_c[k] = c[k] + kRho * (verts[n][k] - c[k]);
                double f_c = objective(std::span<const double>(x_c));
                ++neval;
                if (!std::isfinite(f_c)) f_c = 1e30;
                if (f_c < f_worst) {
                    verts[n] = std::move(x_c);
                    fval[n]  = f_c;
                    accepted = true;
                }
            }

            if (!accepted) {
                // Shrink: x_i = x_best + sigma*(x_i - x_best)
                for (int i = 1; i < m; ++i) {
                    for (int k = 0; k < n; ++k)
                        verts[i][k] = verts[0][k] + kSigma * (verts[i][k] - verts[0][k]);
                    fval[i] = objective(std::span<const double>(verts[i]));
                    ++neval;
                    if (!std::isfinite(fval[i])) fval[i] = 1e30;
                }
            }
        }
    }

    // Final sort to find best.
    sortSimplex(verts, fval);
    return {verts[0], fval[0], maxIterations, neval, OptimizationStatus::MaxIterations};
}

} // namespace ltc
