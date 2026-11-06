#pragma once
#include "FittingConfig.h"
#include "FittingResult.h"
#include "Math.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ltc {

// Full-precision LTC fitting result for one (roughness, view) grid cell.
struct LUTCell
{
    LTCParameters parameters;

    // Full forward and inverse matrices, pre-computed from parameters.
    Mat3 matrix     {Scalar(1)};
    Mat3 inverseMatrix{Scalar(1)};

    double magnitude            = 0.0;
    double fresnelCoefficient   = 0.0;
    bool   hasFresnelCoefficient= false;

    double   initialError       = 0.0;
    double   finalError         = 0.0;
    uint32_t iterations         = 0;
    uint64_t evaluations        = 0;

    FitStatus status            = FitStatus::InvalidInput;

    // True after the cell has been successfully fitted (regardless of FitStatus).
    bool completed              = false;
};

// Metadata stored alongside the LUT cells.
struct LUTMetadata
{
    std::string   brdfName;
    uint32_t      width           = 0;
    uint32_t      height          = 0;
    FittingProfile fittingProfile  = FittingProfile::Reference;
    uint32_t      sampleResolution = 32;
    uint32_t      maxIterations    = 100;
};

// In-memory LTC LUT.  Cells are stored in row-major order:
//   index = roughnessIndex + viewIndex * width
class LUTData
{
public:
    LUTData() = default;
    LUTData(uint32_t width, uint32_t height, LUTMetadata metadata);

    LUTCell&       at(uint32_t roughnessIndex, uint32_t viewIndex);
    const LUTCell& at(uint32_t roughnessIndex, uint32_t viewIndex) const;

    uint32_t width()      const noexcept { return m_width;  }
    uint32_t height()     const noexcept { return m_height; }
    size_t   totalCells() const noexcept { return m_cells.size(); }

    const LUTMetadata& metadata() const noexcept { return m_meta; }

    // Raw span over all cells in storage order.
    const std::vector<LUTCell>& cells() const noexcept { return m_cells; }
          std::vector<LUTCell>& cells()       noexcept { return m_cells; }

private:
    uint32_t            m_width  = 0;
    uint32_t            m_height = 0;
    std::vector<LUTCell> m_cells;
    LUTMetadata         m_meta;
};

} // namespace ltc
