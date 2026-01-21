/**
 * @file ae_simd.c
 * @brief SIMD-optimized buffer operations (SSE2/AVX2)
 */

#include "ae_internal.h"

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#include <emmintrin.h>
#define AE_HAS_SSE2 1
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#define AE_HAS_AVX2 1
#endif

/**
 * Vector addition: dst = a + b
 */
AE_API void ae_simd_add(float *dst, const float *a, const float *b, size_t n) {
  if (!dst || !a || !b)
    return;

#ifdef AE_HAS_SSE2
  size_t i = 0;
  /* Process 4 floats at a time with SSE2 */
  for (; i + 4 <= n; i += 4) {
    __m128 va = _mm_loadu_ps(a + i);
    __m128 vb = _mm_loadu_ps(b + i);
    __m128 result = _mm_add_ps(va, vb);
    _mm_storeu_ps(dst + i, result);
  }
  /* Handle remainder */
  for (; i < n; ++i) {
    dst[i] = a[i] + b[i];
  }
#else
  for (size_t i = 0; i < n; ++i) {
    dst[i] = a[i] + b[i];
  }
#endif
}

/**
 * Vector multiplication: dst = a * b
 */
AE_API void ae_simd_mul(float *dst, const float *a, const float *b, size_t n) {
  if (!dst || !a || !b)
    return;

#ifdef AE_HAS_SSE2
  size_t i = 0;
  for (; i + 4 <= n; i += 4) {
    __m128 va = _mm_loadu_ps(a + i);
    __m128 vb = _mm_loadu_ps(b + i);
    __m128 result = _mm_mul_ps(va, vb);
    _mm_storeu_ps(dst + i, result);
  }
  for (; i < n; ++i) {
    dst[i] = a[i] * b[i];
  }
#else
  for (size_t i = 0; i < n; ++i) {
    dst[i] = a[i] * b[i];
  }
#endif
}

/**
 * Vector scale: dst = src * scale
 */
AE_API void ae_simd_scale(float *dst, const float *src, float scale, size_t n) {
  if (!dst || !src)
    return;

#ifdef AE_HAS_SSE2
  size_t i = 0;
  __m128 vscale = _mm_set1_ps(scale);
  for (; i + 4 <= n; i += 4) {
    __m128 vs = _mm_loadu_ps(src + i);
    __m128 result = _mm_mul_ps(vs, vscale);
    _mm_storeu_ps(dst + i, result);
  }
  for (; i < n; ++i) {
    dst[i] = src[i] * scale;
  }
#else
  for (size_t i = 0; i < n; ++i) {
    dst[i] = src[i] * scale;
  }
#endif
}

/**
 * Multiply-accumulate: dst += a * b
 */
AE_API void ae_simd_mac(float *dst, const float *a, const float *b, size_t n) {
  if (!dst || !a || !b)
    return;

#ifdef AE_HAS_SSE2
  size_t i = 0;
  for (; i + 4 <= n; i += 4) {
    __m128 vd = _mm_loadu_ps(dst + i);
    __m128 va = _mm_loadu_ps(a + i);
    __m128 vb = _mm_loadu_ps(b + i);
    __m128 product = _mm_mul_ps(va, vb);
    __m128 result = _mm_add_ps(vd, product);
    _mm_storeu_ps(dst + i, result);
  }
  for (; i < n; ++i) {
    dst[i] += a[i] * b[i];
  }
#else
  for (size_t i = 0; i < n; ++i) {
    dst[i] += a[i] * b[i];
  }
#endif
}

/**
 * Vector copy with gain: dst = src * gain
 * In-place safe (dst == src)
 */
void ae_simd_copy_gain(float *dst, const float *src, float gain, size_t n) {
  ae_simd_scale(dst, src, gain, n);
}

/**
 * Mix with gain: dst += src * gain
 */
void ae_simd_mix_gain(float *dst, const float *src, float gain, size_t n) {
  if (!dst || !src)
    return;

#ifdef AE_HAS_SSE2
  size_t i = 0;
  __m128 vgain = _mm_set1_ps(gain);
  for (; i + 4 <= n; i += 4) {
    __m128 vd = _mm_loadu_ps(dst + i);
    __m128 vs = _mm_loadu_ps(src + i);
    __m128 scaled = _mm_mul_ps(vs, vgain);
    __m128 result = _mm_add_ps(vd, scaled);
    _mm_storeu_ps(dst + i, result);
  }
  for (; i < n; ++i) {
    dst[i] += src[i] * gain;
  }
#else
  for (size_t i = 0; i < n; ++i) {
    dst[i] += src[i] * gain;
  }
#endif
}

/**
 * Interleave stereo: dst[LRLRLR...] = left[LLL...], right[RRR...]
 */
void ae_simd_interleave_stereo(float *dst, const float *left,
                               const float *right, size_t frames) {
  if (!dst || !left || !right)
    return;

  for (size_t i = 0; i < frames; ++i) {
    dst[i * 2] = left[i];
    dst[i * 2 + 1] = right[i];
  }
}

/**
 * Deinterleave stereo: left[LLL...], right[RRR...] = src[LRLRLR...]
 */
void ae_simd_deinterleave_stereo(float *left, float *right, const float *src,
                                 size_t frames) {
  if (!left || !right || !src)
    return;

  for (size_t i = 0; i < frames; ++i) {
    left[i] = src[i * 2];
    right[i] = src[i * 2 + 1];
  }
}

/**
 * Find maximum absolute value in buffer
 */
float ae_simd_max_abs(const float *src, size_t n) {
  if (!src || n == 0)
    return 0.0f;

#ifdef AE_HAS_SSE2
  size_t i = 0;
  __m128 vmax = _mm_setzero_ps();
  __m128 sign_mask = _mm_set1_ps(-0.0f);

  for (; i + 4 <= n; i += 4) {
    __m128 v = _mm_loadu_ps(src + i);
    __m128 vabs = _mm_andnot_ps(sign_mask, v); /* abs */
    vmax = _mm_max_ps(vmax, vabs);
  }

  /* Horizontal max */
  float result[4];
  _mm_storeu_ps(result, vmax);
  float max_val =
      fmaxf(fmaxf(result[0], result[1]), fmaxf(result[2], result[3]));

  /* Handle remainder */
  for (; i < n; ++i) {
    float abs_val = fabsf(src[i]);
    if (abs_val > max_val)
      max_val = abs_val;
  }
  return max_val;
#else
  float max_val = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    float abs_val = fabsf(src[i]);
    if (abs_val > max_val)
      max_val = abs_val;
  }
  return max_val;
#endif
}
