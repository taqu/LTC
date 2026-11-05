#include <gtest/gtest.h>
#include "ltc/ParameterGrid.h"
#include <cmath>

using namespace ltc;

// ---- Construction ----

TEST(ParameterGrid, InvalidDimensionsThrow)
{
    EXPECT_THROW(ParameterGrid(1, 64), std::invalid_argument);
    EXPECT_THROW(ParameterGrid(64, 1), std::invalid_argument);
    EXPECT_THROW(ParameterGrid(0, 64), std::invalid_argument);
    EXPECT_THROW(ParameterGrid(64, 0), std::invalid_argument);
    EXPECT_NO_THROW(ParameterGrid(2, 2));
}

// ---- Roughness mapping ----

TEST(ParameterGrid, FirstRoughnessIsSmooth)
{
    ParameterGrid g(64, 64);
    // a=0 → r=0, alpha = max(0, 1e-5) = 1e-5
    EXPECT_DOUBLE_EQ(g.roughnessAt(0), 0.0);
    EXPECT_DOUBLE_EQ(g.alphaAt(0), kMinAlpha);
}

TEST(ParameterGrid, LastRoughnessIsOne)
{
    ParameterGrid g(64, 64);
    // a=63 → r=1, alpha=1
    EXPECT_DOUBLE_EQ(g.roughnessAt(63), 1.0);
    EXPECT_NEAR(g.alphaAt(63), 1.0, 1e-15);
}

TEST(ParameterGrid, AlphaFloor)
{
    ParameterGrid g(64, 64);
    for (uint32_t a = 0; a < 64; ++a)
        EXPECT_GE(g.alphaAt(a), kMinAlpha);
}

TEST(ParameterGrid, AlphaMonotone)
{
    ParameterGrid g(64, 64);
    for (uint32_t a = 1; a < 64; ++a)
        EXPECT_GE(g.alphaAt(a), g.alphaAt(a - 1));
}

TEST(ParameterGrid, AlphaIsRSquared)
{
    ParameterGrid g(64, 64);
    // For a where r^2 > 1e-5
    for (uint32_t a = 4; a < 64; ++a) {
        double r = g.roughnessAt(a);
        EXPECT_NEAR(g.alphaAt(a), r * r, 1e-14);
    }
}

// ---- View-angle mapping ----

TEST(ParameterGrid, FirstViewIsNormalIncidence)
{
    ParameterGrid g(64, 64);
    // t=0 → x=0, cosTheta=1, theta=0, V=(0,0,1)
    EXPECT_NEAR(g.cosThetaAt(0), 1.0, 1e-14);
    EXPECT_NEAR(g.thetaAt(0),    0.0, 1e-14);
    Vec3 v = g.viewAt(0);
    EXPECT_NEAR(v.x, 0.0, 1e-14);
    EXPECT_NEAR(v.y, 0.0, 1e-14);
    EXPECT_NEAR(v.z, 1.0, 1e-14);
}

TEST(ParameterGrid, LastViewClampedToGrazing)
{
    ParameterGrid g(64, 64);
    // t=63 → x=1, cosTheta=0, theta clamped to 1.57
    EXPECT_NEAR(g.cosThetaAt(63), 0.0,  1e-14);
    EXPECT_NEAR(g.thetaAt(63),    1.57, 1e-14);

    Vec3 v = g.viewAt(63);
    double expected_sin = std::sin(1.57);
    double expected_cos = std::cos(1.57);
    EXPECT_NEAR(v.x, expected_sin, 1e-14);
    EXPECT_NEAR(v.y, 0.0,          1e-14);
    EXPECT_NEAR(v.z, expected_cos, 1e-14);
}

TEST(ParameterGrid, ThetaMonotone)
{
    ParameterGrid g(64, 64);
    for (uint32_t t = 1; t < 64; ++t)
        EXPECT_GE(g.thetaAt(t), g.thetaAt(t - 1));
}

TEST(ParameterGrid, ViewYIsAlwaysZero)
{
    ParameterGrid g(64, 64);
    for (uint32_t t = 0; t < 64; ++t)
        EXPECT_NEAR(g.viewAt(t).y, 0.0, 1e-14);
}

TEST(ParameterGrid, ViewDirectionIsNormalized)
{
    ParameterGrid g(64, 64);
    for (uint32_t t = 0; t < 64; ++t) {
        Vec3 v = g.viewAt(t);
        double len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
        EXPECT_NEAR(len, 1.0, 1e-13);
    }
}

TEST(ParameterGrid, ThetaNotExactlyHalfPi)
{
    // The spec requires 1.57, NOT pi/2 ≈ 1.5707963...
    ParameterGrid g(64, 64);
    EXPECT_DOUBLE_EQ(g.thetaAt(63), 1.57);
    EXPECT_NE(g.thetaAt(63), 3.14159265358979323846 / 2.0);
}

// ---- Indexing ----

TEST(ParameterGrid, IndexFormula)
{
    ParameterGrid g(8, 4);
    for (uint32_t t = 0; t < 4; ++t)
        for (uint32_t a = 0; a < 8; ++a)
            EXPECT_EQ(g.index({a, t}), a + t * 8u);
}

TEST(ParameterGrid, EvaluateMatchesHelpers)
{
    ParameterGrid g(16, 16);
    for (uint32_t t = 0; t < 16; ++t) {
        for (uint32_t a = 0; a < 16; ++a) {
            LUTParameters p = g.evaluate({a, t});
            EXPECT_DOUBLE_EQ(p.alpha,         g.alphaAt(a));
            EXPECT_DOUBLE_EQ(p.ndotv,         g.cosThetaAt(t));
            EXPECT_DOUBLE_EQ(p.theta,         g.thetaAt(t));
            EXPECT_DOUBLE_EQ(p.viewDirection.x, g.viewAt(t).x);
            EXPECT_DOUBLE_EQ(p.viewDirection.z, g.viewAt(t).z);
        }
    }
}

// ---- Non-square grid ----

TEST(ParameterGrid, NonSquareGrid)
{
    EXPECT_NO_THROW(ParameterGrid(32, 8));
    ParameterGrid g(32, 8);
    EXPECT_EQ(g.width(),  32u);
    EXPECT_EQ(g.height(),  8u);
    EXPECT_DOUBLE_EQ(g.roughnessAt(31), 1.0);
    EXPECT_DOUBLE_EQ(g.cosThetaAt(7), 0.0);
}

// ---- Intermediate values ----

TEST(ParameterGrid, IntermediateRoughness)
{
    ParameterGrid g(64, 64);
    // a=32 → r = 32/63
    double r = 32.0 / 63.0;
    EXPECT_NEAR(g.roughnessAt(32), r, 1e-14);
    EXPECT_NEAR(g.alphaAt(32), r * r, 1e-14);
}

TEST(ParameterGrid, IntermediateViewAngle)
{
    ParameterGrid g(64, 64);
    // t=32 → x = 32/63, cosTheta = 1 - x^2
    double x = 32.0 / 63.0;
    double expected_cos = 1.0 - x * x;
    EXPECT_NEAR(g.cosThetaAt(32), expected_cos, 1e-14);
}
