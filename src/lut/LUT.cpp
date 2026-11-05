#include "ltc/LUT.h"
#include <stdexcept>

namespace ltc {

LUTData::LUTData(uint32_t width, uint32_t height, LUTMetadata metadata)
    : m_width(width)
    , m_height(height)
    , m_cells(static_cast<size_t>(width) * height)
    , m_meta(std::move(metadata))
{
}

LUTCell& LUTData::at(uint32_t a, uint32_t t)
{
    return m_cells[static_cast<size_t>(a) + static_cast<size_t>(t) * m_width];
}

const LUTCell& LUTData::at(uint32_t a, uint32_t t) const
{
    return m_cells[static_cast<size_t>(a) + static_cast<size_t>(t) * m_width];
}

} // namespace ltc
