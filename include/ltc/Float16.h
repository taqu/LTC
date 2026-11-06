#pragma once
#include <cstdint>

namespace ltc {

// IEEE 754 binary16 encode, round-to-nearest-ties-to-even
uint16_t float32ToFloat16(float value);

} // namespace ltc
