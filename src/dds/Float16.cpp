#include "ltc/Float16.h"
#include <cstring>
#include <cstdint>

namespace ltc {

uint16_t float32ToFloat16(float value) {
    // Bitcast float to uint32_t
    uint32_t x = 0;
    std::memcpy(&x, &value, sizeof(x));

    uint32_t sign    = x >> 31;
    uint32_t exp32   = (x >> 23) & 0xFF;
    uint32_t mant32  = x & 0x7FFFFF;

    // NaN: exponent all ones, mantissa non-zero
    if (exp32 == 255 && mant32 != 0) {
        // Preserve NaN, ensure mantissa is non-zero
        uint32_t mant16 = mant32 >> 13;
        if (mant16 == 0) mant16 = 1;
        return static_cast<uint16_t>((sign << 15) | 0x7C00 | mant16);
    }

    // Inf: exponent all ones, mantissa zero
    if (exp32 == 255) {
        return static_cast<uint16_t>((sign << 15) | 0x7C00);
    }

    // Zero or FP32 denormal (exp32 == 0): map to FP16 zero
    if (exp32 == 0) {
        return static_cast<uint16_t>(sign << 15);
    }

    // Normal FP32
    int32_t exp16 = static_cast<int32_t>(exp32) - 127 + 15;

    if (exp16 >= 31) {
        // Overflow to Inf
        return static_cast<uint16_t>((sign << 15) | 0x7C00);
    }

    if (exp16 <= 0) {
        // Underflow to FP16 subnormal or zero
        // The implicit leading 1 bit makes full mantissa = (1 << 23) | mant32
        // We need to shift right by (1 - exp16 + 13) = (14 - exp16) positions
        int32_t shift = 14 - exp16; // = 1 - exp16 + 13

        if (shift >= 25) {
            // Too small, underflows to zero
            return static_cast<uint16_t>(sign << 15);
        }

        uint32_t full  = (1u << 23) | mant32;
        uint32_t mant16 = full >> shift;

        // Round-to-nearest-ties-to-even
        uint32_t guard  = (full >> (shift - 1)) & 1;
        uint32_t sticky = (shift >= 2) ? (full & ((1u << (shift - 1)) - 1)) : 0;
        uint32_t lsb    = mant16 & 1;

        if (guard && (sticky || lsb)) {
            mant16++;
        }

        // Check if rounding caused carry into exponent
        if (mant16 >= (1u << 10)) {
            // carry: exponent becomes 1, mantissa becomes 0
            return static_cast<uint16_t>((sign << 15) | (1u << 10));
        }

        return static_cast<uint16_t>((sign << 15) | mant16);
    }

    // Normal FP16
    uint32_t mant16 = mant32 >> 13;
    uint32_t guard  = (mant32 >> 12) & 1;
    uint32_t sticky = mant32 & 0xFFF;
    uint32_t lsb    = mant16 & 1;

    if (guard && (sticky || lsb)) {
        mant16++;
        if (mant16 >= (1u << 10)) {
            mant16 = 0;
            exp16++;
            if (exp16 >= 31) {
                // Overflow to Inf
                return static_cast<uint16_t>((sign << 15) | 0x7C00);
            }
        }
    }

    return static_cast<uint16_t>((sign << 15) | (static_cast<uint32_t>(exp16) << 10) | mant16);
}

} // namespace ltc
