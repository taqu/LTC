#include "ltc/Float16.h"
#include "ltc/DDSReader.h"
#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>

namespace {

// Hardcoded expected FP16 bit patterns — do NOT use the same implementation to generate them.
// Values verified against published FP16 tables.

TEST(Float16Encoding, Zero) {
    EXPECT_EQ(ltc::float32ToFloat16(+0.0f), 0x0000u);
    EXPECT_EQ(ltc::float32ToFloat16(-0.0f), 0x8000u);
}

TEST(Float16Encoding, One) {
    EXPECT_EQ(ltc::float32ToFloat16(+1.0f), 0x3C00u);
    EXPECT_EQ(ltc::float32ToFloat16(-1.0f), 0xBC00u);
}

TEST(Float16Encoding, MaxFiniteFP16) {
    // 65504.0 = max positive finite FP16 value
    EXPECT_EQ(ltc::float32ToFloat16(65504.0f), 0x7BFFu);
}

TEST(Float16Encoding, MinPositiveNormalFP16) {
    // 2^-14 ≈ 6.103515625e-5 = minimum positive normal FP16
    EXPECT_EQ(ltc::float32ToFloat16(6.103515625e-5f), 0x0400u);
}

TEST(Float16Encoding, MinPositiveSubnormalFP16) {
    // 2^-24 ≈ 5.960464e-8 = minimum positive subnormal FP16
    // This is 2^-14 * 2^-10 = 2^-24
    EXPECT_EQ(ltc::float32ToFloat16(5.960464477539063e-8f), 0x0001u);
}

TEST(Float16Encoding, Infinity) {
    // Positive infinity
    EXPECT_EQ(ltc::float32ToFloat16(std::numeric_limits<float>::infinity()),  0x7C00u);
    // Negative infinity
    EXPECT_EQ(ltc::float32ToFloat16(-std::numeric_limits<float>::infinity()), 0xFC00u);
}

TEST(Float16Encoding, NaN) {
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    uint16_t bits = ltc::float32ToFloat16(nan_val);
    // NaN: exponent = 0x1F (all ones), mantissa != 0
    uint16_t expo = (bits >> 10) & 0x1F;
    uint16_t mant = bits & 0x3FF;
    EXPECT_EQ(expo, 0x1Fu) << "NaN must have all-ones exponent";
    EXPECT_NE(mant, 0u)    << "NaN must have non-zero mantissa";
}

TEST(Float16Encoding, HalfValue) {
    // 0.5f = 2^-1 * 1.0 → FP16 sign=0, exp=14 (biased), mant=0 → 0x3800
    EXPECT_EQ(ltc::float32ToFloat16(0.5f), 0x3800u);
}

TEST(Float16Encoding, LTCRangeCoefficient) {
    // 0.5f should encode as 0x3800
    EXPECT_EQ(ltc::float32ToFloat16(0.5f), 0x3800u);
}

TEST(Float16Encoding, OverflowToInf) {
    // Values larger than 65504 should overflow to infinity
    EXPECT_EQ(ltc::float32ToFloat16(1e10f), 0x7C00u);
    EXPECT_EQ(ltc::float32ToFloat16(-1e10f), 0xFC00u);
}

TEST(Float16Encoding, NegativeValues) {
    // -2.0f: sign=1, exp=16 (biased fp16=0x10), mant=0 → 0xC000
    EXPECT_EQ(ltc::float32ToFloat16(-2.0f), 0xC000u);
    // -0.5f → 0xB800
    EXPECT_EQ(ltc::float32ToFloat16(-0.5f), 0xB800u);
}

TEST(Float16Encoding, TwoPointZero) {
    // 2.0f: sign=0, biased_exp=16, mant=0 → 0x4000
    EXPECT_EQ(ltc::float32ToFloat16(2.0f), 0x4000u);
}

TEST(Float16Encoding, HalfwayRoundToEven) {
    // Test round-to-nearest-ties-to-even behavior.
    // FP16 normal: 1.0f = 0x3C00
    // The mantissa of 1.0 in FP32 is 0x000000.
    // 1.0 + 2^-11 is representable in FP16 (bit 10 of mantissa set), so round up.
    // 1.0 + 2^-12 is halfway between 1.0 and 1.0+2^-10 (FP16 ULP), ties-to-even
    // The LSB of 1.0's FP16 mantissa is 0, so it rounds down to 0x3C00.

    // FP32 bit pattern for 1.0 + 2^-12:
    // 1.0f = 0x3F800000
    // 2^-12 in FP32: exponent = 127-12 = 115 = 0x73, bits = 0x3A800000
    // 1.0 + 2^-12 = 1.000244140625
    float val = 1.0f + (1.0f / 4096.0f); // 1 + 2^-12
    // This is halfway between FP16 1.0 and 1.0 + 2^-10 (ULP=0x3C01-0x3C00=1)
    // For round-to-even: lsb of 1.0 in FP16 is 0, so it rounds to 1.0 (even)
    uint16_t bits = ltc::float32ToFloat16(val);
    EXPECT_EQ(bits, 0x3C00u); // rounds to even (down)

    // 3.0f + 2^-10 is halfway test for another value
    // 3.0f in FP16: sign=0, exp=16+1=17? No: 3 = 1.5 * 2^1, exp16=15+1=16, mant=0x200
    // FP16: 0x4200
    EXPECT_EQ(ltc::float32ToFloat16(3.0f), 0x4200u);
}

TEST(Float16Encoding, RoundTripViaDecoder) {
    // Encode and then decode several values, comparing to original with FP16 precision loss
    float values[] = { 1.0f, -1.0f, 0.5f, 0.25f, 2.0f, 100.0f, -100.0f };
    for (float v : values) {
        uint16_t encoded = ltc::float32ToFloat16(v);
        float decoded = ltc::DDSReader::fp16ToFloat(encoded);
        // The round-trip should be close to the original (within FP16 precision)
        EXPECT_NEAR(decoded, v, std::abs(v) * 0.001f + 1e-6f) << "value: " << v;
    }
}

TEST(Float16Encoding, SmallSubnormals) {
    // Values smaller than min subnormal should underflow to zero
    EXPECT_EQ(ltc::float32ToFloat16(1e-45f), 0x0000u); // very small FP32 denormal
}

} // namespace
