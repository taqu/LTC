#ifndef INC_FLOAT_TO_HALF_H_
#define INC_FLOAT_TO_HALF_H_
#include <cstdint>
#include <immintrin.h>

inline uint16_t float_to_half(float x)
{
    __m128 m =_mm_set1_ps(x);
    __m128i half_vec = _mm_cvtps_ph(m, 0);
    uint16_t half_array[4];
    _mm_storel_epi64(reinterpret_cast<__m128i*>(half_array), half_vec);
    return half_array[0];
}

inline void float_to_half(uint16_t dst[4], const float src[4])
{
    __m128 m = _mm_loadu_ps(src);
    __m128i half_vec = _mm_cvtps_ph(m, 0);
    _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), half_vec);
}

inline float half_to_float(uint16_t x)
{
    __m128i src = _mm_setr_epi16(x, 0, 0, 0, 0, 0, 0, 0);
    __m128 dst = _mm_cvtph_ps(src);
    return _mm_cvtss_f32(dst);
}

inline void half_to_float(float dst[4], const uint16_t src[4])
{
    __m128i m0 = _mm_loadu_si64(src);
    __m128 m1 = _mm_cvtph_ps(m0);
    _mm_storeu_ps(dst, m1);
}

#endif //INC_FLOAT_TO_HALF_H_

