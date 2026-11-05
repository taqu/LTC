#pragma once
#include "Math.h"
#include <cstdint>
#include <stdexcept>

namespace ltc {

// A direction sampled from a distribution together with its PDF.
struct DirectionSample {
    Vec3   direction; // unit direction
    Scalar pdf;       // solid-angle PDF at direction
};

// Sample a direction from the cosine-weighted hemisphere (z >= 0).
// u must be in [0, 1)^2.  Returns pdf = cos(theta)/pi.
DirectionSample sampleCosineHemisphere(const Vec2& u) noexcept;

// PDF of the cosine-weighted hemisphere distribution at the given direction.
// Returns 0 for directions with z <= 0.
Scalar cosineHemispherePdf(const Vec3& direction) noexcept;

// ---- Deterministic midpoint sample grid ----

// A 2D sample from a midpoint grid.
struct Sample2D {
    Scalar u1; // (x + 0.5) / N
    Scalar u2; // (y + 0.5) / N
};

// Regular N×N midpoint grid over [0,1)^2.
// u1 = (x + 0.5) / N,  u2 = (y + 0.5) / N
// Throws std::invalid_argument when resolution == 0.
class SampleGrid {
public:
    explicit SampleGrid(uint32_t resolution);

    uint32_t resolution() const noexcept { return m_resolution; }

    // Returns the midpoint sample at grid cell (x, y).
    // x and y must be in [0, resolution).
    Sample2D sample(uint32_t x, uint32_t y) const noexcept;

private:
    uint32_t m_resolution;
};

} // namespace ltc
