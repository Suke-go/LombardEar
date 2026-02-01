#ifndef FIXED_POINT_H
#define FIXED_POINT_H

/**
 * Fixed-Point Arithmetic for STM32F4
 *
 * Q15: 16-bit signed, 15 fractional bits (range: -1.0 to ~0.9999)
 * Q31: 32-bit signed, 31 fractional bits (range: -1.0 to ~0.9999)
 *
 * STM32F4 with FPU: float is faster for most operations
 * STM32F4 without FPU: Q15/Q31 is essential
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Type Definitions
// ============================================================================

typedef int16_t q15_t;
typedef int32_t q31_t;

// ============================================================================
// Conversion Macros
// ============================================================================

#define Q15_MAX ((q15_t)0x7FFF)
#define Q15_MIN ((q15_t)0x8000)
#define Q31_MAX ((q31_t)0x7FFFFFFF)
#define Q31_MIN ((q31_t)0x80000000)

// Float to Q15 (with saturation)
static inline q15_t float_to_q15(float x) {
  if (x >= 1.0f)
    return Q15_MAX;
  if (x < -1.0f)
    return Q15_MIN;
  return (q15_t)(x * 32768.0f);
}

// Q15 to float
static inline float q15_to_float(q15_t x) { return (float)x / 32768.0f; }

// Float to Q31 (with saturation)
static inline q31_t float_to_q31(float x) {
  if (x >= 1.0f)
    return Q31_MAX;
  if (x < -1.0f)
    return Q31_MIN;
  return (q31_t)(x * 2147483648.0f);
}

// Q31 to float
static inline float q31_to_float(q31_t x) { return (float)x / 2147483648.0f; }

// ============================================================================
// Q15 Arithmetic
// ============================================================================

// Q15 add with saturation
static inline q15_t q15_add(q15_t a, q15_t b) {
  int32_t sum = (int32_t)a + (int32_t)b;
  if (sum > Q15_MAX)
    return Q15_MAX;
  if (sum < Q15_MIN)
    return Q15_MIN;
  return (q15_t)sum;
}

// Q15 multiply (result is Q15)
static inline q15_t q15_mul(q15_t a, q15_t b) {
  int32_t prod = ((int32_t)a * (int32_t)b) >> 15;
  if (prod > Q15_MAX)
    return Q15_MAX;
  if (prod < Q15_MIN)
    return Q15_MIN;
  return (q15_t)prod;
}

// Q15 MAC (multiply-accumulate): acc += a * b
static inline q31_t q15_mac(q31_t acc, q15_t a, q15_t b) {
  return acc + ((int32_t)a * (int32_t)b);
}

// ============================================================================
// Q31 Arithmetic
// ============================================================================

// Q31 add with saturation
static inline q31_t q31_add(q31_t a, q31_t b) {
  int64_t sum = (int64_t)a + (int64_t)b;
  if (sum > Q31_MAX)
    return Q31_MAX;
  if (sum < Q31_MIN)
    return Q31_MIN;
  return (q31_t)sum;
}

// Q31 multiply (result is Q31)
static inline q31_t q31_mul(q31_t a, q31_t b) {
  int64_t prod = ((int64_t)a * (int64_t)b) >> 31;
  if (prod > Q31_MAX)
    return Q31_MAX;
  if (prod < Q31_MIN)
    return Q31_MIN;
  return (q31_t)prod;
}

// ============================================================================
// Batch Conversion (for buffer processing)
// ============================================================================

// Convert float buffer to Q15
static inline void float_to_q15_batch(const float *src, q15_t *dst, int n) {
  for (int i = 0; i < n; i++) {
    dst[i] = float_to_q15(src[i]);
  }
}

// Convert Q15 buffer to float
static inline void q15_to_float_batch(const q15_t *src, float *dst, int n) {
  for (int i = 0; i < n; i++) {
    dst[i] = q15_to_float(src[i]);
  }
}

// ============================================================================
// Fixed-Point Biquad Filter
// ============================================================================

typedef struct {
  q15_t b0, b1, b2; // Numerator coefficients
  q15_t a1, a2;     // Denominator coefficients (negated)
  q15_t z1, z2;     // State variables
} BiquadQ15State;

// Initialize from float coefficients
static inline void biquad_q15_init(BiquadQ15State *st, float b0, float b1,
                                   float b2, float a1, float a2) {
  st->b0 = float_to_q15(b0);
  st->b1 = float_to_q15(b1);
  st->b2 = float_to_q15(b2);
  st->a1 = float_to_q15(-a1); // Negate for direct form
  st->a2 = float_to_q15(-a2);
  st->z1 = 0;
  st->z2 = 0;
}

// Process one sample
static inline q15_t biquad_q15_process(BiquadQ15State *st, q15_t in) {
  // Direct Form II Transposed
  q31_t acc = q15_mac(0, st->b0, in);
  acc = q15_mac(acc, st->z1, 1);
  q15_t out = (q15_t)(acc >> 15);

  // Update state
  q31_t z1_new = q15_mac(0, st->b1, in);
  z1_new = q15_mac(z1_new, st->a1, out);
  z1_new = q15_mac(z1_new, st->z2, 1);

  q31_t z2_new = q15_mac(0, st->b2, in);
  z2_new = q15_mac(z2_new, st->a2, out);

  st->z1 = (q15_t)(z1_new >> 15);
  st->z2 = (q15_t)(z2_new >> 15);

  return out;
}

#ifdef __cplusplus
}
#endif

#endif // FIXED_POINT_H
