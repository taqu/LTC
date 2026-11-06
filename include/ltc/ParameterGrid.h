#pragma once
#include "Math.h"
#include <cstdint>
#include <stdexcept>

namespace ltc {

struct LUTCoordinates
{
    uint32_t roughnessIndex = 0;
    uint32_t viewIndex      = 0;
};

// All derived physical parameters for one LUT cell.
struct LUTParameters
{
    double perceptualRoughness = 0.0; // r  = a / (W-1)
    double alpha               = 0.0; // max(r^2, 1e-5)
    double ndotv               = 1.0; // cos(theta)
    double theta               = 0.0; // view angle (radians)
    Vec3   viewDirection{0.0, 0.0, 1.0};
};

// Minimum GGX alpha used in the reference profile.
inline constexpr double kMinAlpha = 1e-5;

// Two-dimensional LUT parameter grid following the reference parameterization.
//
// Roughness axis (X):  index a in [0, width-1]
//   r     = a / (width - 1)
//   alpha = max(r^2, MinAlpha)
//
// View-angle axis (Y):  index t in [0, height-1]
//   x        = t / (height - 1)
//   cosTheta = 1 - x^2
//   theta    = min(acos(cosTheta), 1.57)
//   V        = (sin(theta), 0, cos(theta))
//
// Storage:  index = roughnessIndex + viewIndex * width
//
// Both width and height must be at least 2.
class ParameterGrid
{
public:
    ParameterGrid(uint32_t width, uint32_t height);

    LUTParameters evaluate(LUTCoordinates coords) const;

    uint32_t width()  const noexcept { return m_width;  }
    uint32_t height() const noexcept { return m_height; }

    size_t index(LUTCoordinates coords) const;

    // Helpers
    double roughnessAt(uint32_t a) const noexcept;
    double alphaAt    (uint32_t a) const noexcept;
    double cosThetaAt (uint32_t t) const noexcept;
    double thetaAt    (uint32_t t) const noexcept;
    Vec3   viewAt     (uint32_t t) const noexcept;

private:
    uint32_t m_width;
    uint32_t m_height;
};

} // namespace ltc
