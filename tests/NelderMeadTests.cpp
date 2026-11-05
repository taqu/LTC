#include <gtest/gtest.h>
#include "ltc/NelderMead.h"
#include <array>
#include <cmath>

using namespace ltc;

// ---- 1-D quadratic ----

TEST(NelderMead, Quadratic1D)
{
    NelderMead nm;
    // f(x) = (x - 2)^2 — minimum at x = 2
    auto f = [](std::span<const double> p) { return (p[0] - 2.0) * (p[0] - 2.0); };

    double init[1] = {0.0};
    // step=1.0 avoids the symmetry trap where both simplex vertices land
    // equidistant from the minimum (f(1.5)==f(2.5)==0.25 with step=0.5).
    auto r = nm.minimize(f, std::span<const double>(init, 1), 1.0, 1e-8, 500);

    EXPECT_EQ(r.status, OptimizationStatus::Converged);
    EXPECT_NEAR(r.parameters[0], 2.0, 1e-4);
    EXPECT_NEAR(r.objectiveValue, 0.0, 1e-7);
    EXPECT_GT(r.evaluations, 0u);
}

// ---- 3-D quadratic ----

TEST(NelderMead, Quadratic3D)
{
    NelderMead nm;
    // f(x,y,z) = x^2 + y^2 + z^2 — minimum at (0, 0, 0)
    auto f = [](std::span<const double> p) {
        return p[0]*p[0] + p[1]*p[1] + p[2]*p[2];
    };

    double init[3] = {1.0, 1.0, 1.0};
    auto r = nm.minimize(f, std::span<const double>(init, 3), 0.5, 1e-8, 1000);

    EXPECT_EQ(r.status, OptimizationStatus::Converged);
    EXPECT_NEAR(r.parameters[0], 0.0, 1e-3);
    EXPECT_NEAR(r.parameters[1], 0.0, 1e-3);
    EXPECT_NEAR(r.parameters[2], 0.0, 1e-3);
    EXPECT_NEAR(r.objectiveValue, 0.0, 1e-5);
}

// ---- Rosenbrock (challenging) ----

TEST(NelderMead, RosenbrockErrorDecreases)
{
    NelderMead nm;
    // f(x, y) = 100*(y - x^2)^2 + (1-x)^2 — minimum at (1, 1)
    auto f = [](std::span<const double> p) -> double {
        double x = p[0], y = p[1];
        return 100.0 * (y - x*x) * (y - x*x) + (1.0 - x) * (1.0 - x);
    };

    double init_val = f(std::span<const double>(std::array<double,2>{-1.2, 1.0}));

    double init[2] = {-1.2, 1.0};
    auto r = nm.minimize(f, std::span<const double>(init, 2), 0.1, 1e-8, 2000);

    EXPECT_LT(r.objectiveValue, init_val);
    EXPECT_GT(r.evaluations, 0u);
}

// ---- Constant function — should converge immediately (range = 0) ----

TEST(NelderMead, ConstantObjectiveConverges)
{
    NelderMead nm;
    auto f = [](std::span<const double>) { return 5.0; };

    double init[1] = {1.0};
    auto r = nm.minimize(f, std::span<const double>(init, 1), 0.05, 1e-5, 100);

    // |f_best - f_worst| = 0 < tol → converges on first iteration (or iter 0)
    EXPECT_EQ(r.status, OptimizationStatus::Converged);
    EXPECT_DOUBLE_EQ(r.objectiveValue, 5.0);
}

// ---- Non-finite initial point ----

TEST(NelderMead, InvalidInitialPoint)
{
    NelderMead nm;
    auto f = [](std::span<const double>) -> double { return std::numeric_limits<double>::infinity(); };

    double init[1] = {0.0};
    auto r = nm.minimize(f, std::span<const double>(init, 1), 0.05, 1e-5, 100);

    EXPECT_EQ(r.status, OptimizationStatus::InvalidInitialPoint);
}

// ---- Max iterations ----

TEST(NelderMead, MaxIterationsStatus)
{
    NelderMead nm;
    // A nontrivial function that won't converge in 2 iterations.
    auto f = [](std::span<const double> p) {
        return p[0]*p[0] + p[1]*p[1] + p[2]*p[2] + p[0]*p[1];
    };

    double init[3] = {5.0, 5.0, 5.0};
    auto r = nm.minimize(f, std::span<const double>(init, 3), 0.5, 1e-15, 2);

    EXPECT_EQ(r.status, OptimizationStatus::MaxIterations);
    EXPECT_EQ(r.iterations, 2u);
    EXPECT_GE(r.evaluations, 4u); // at least N+1 initialization calls
}

// ---- Determinism ----

TEST(NelderMead, Deterministic)
{
    NelderMead nm;
    auto f = [](std::span<const double> p) { return p[0]*p[0] + p[1]*p[1]; };

    double init[2] = {3.0, -2.0};
    auto r1 = nm.minimize(f, std::span<const double>(init, 2), 0.1, 1e-8, 500);
    auto r2 = nm.minimize(f, std::span<const double>(init, 2), 0.1, 1e-8, 500);

    EXPECT_EQ(r1.status,       r2.status);
    EXPECT_EQ(r1.evaluations,  r2.evaluations);
    EXPECT_EQ(r1.iterations,   r2.iterations);
    EXPECT_DOUBLE_EQ(r1.objectiveValue, r2.objectiveValue);
    ASSERT_EQ(r1.parameters.size(), r2.parameters.size());
    for (size_t i = 0; i < r1.parameters.size(); ++i)
        EXPECT_DOUBLE_EQ(r1.parameters[i], r2.parameters[i]);
}

// ---- Partial non-finite objective (some evaluations return inf) ----

TEST(NelderMead, PartialNonFiniteObjective)
{
    NelderMead nm;
    // Returns inf for positive x, otherwise x^2.
    auto f = [](std::span<const double> p) -> double {
        if (p[0] > 0.0) return std::numeric_limits<double>::infinity();
        return p[0] * p[0];
    };

    // Start at -2 so initial is valid; NM should stay on negative side.
    double init[1] = {-2.0};
    auto r = nm.minimize(f, std::span<const double>(init, 1), 0.5, 1e-8, 500);

    // Best reachable minimum is x=0 (from the negative side).
    EXPECT_TRUE(r.status == OptimizationStatus::Converged ||
                r.status == OptimizationStatus::MaxIterations);
    EXPECT_NEAR(r.objectiveValue, 0.0, 1e-4);
}
