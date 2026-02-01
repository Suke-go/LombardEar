#ifndef FAST_MATH_H
#define FAST_MATH_H

/**
 * Fast Math Library for Real-Time Audio DSP
 *
 * Optimizations:
 * - LUT for sin/cos (512 entries, ~0.7° resolution)
 * - Fast approximations for sqrt, atan2
 * - SIMD-friendly data structures
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// ============================================================================
// Configuration
// ============================================================================

#define FAST_SIN_TABLE_SIZE 512
#define FAST_SIN_TABLE_MASK (FAST_SIN_TABLE_SIZE - 1)

// ============================================================================
// LUT-based Trigonometry
// ============================================================================

// Pre-computed sine table (initialized at startup)
extern float fast_sin_table[FAST_SIN_TABLE_SIZE];

// Initialize LUT (call once at startup)
void fast_math_init(void);

/**
 * Fast sine using LUT with linear interpolation.
 * @param x  Angle in radians
 * @return   sin(x), accuracy ~0.001
 */
static inline float fast_sinf(float x) {
  // Normalize to [0, 2π)
  const float TWO_PI = 6.28318530718f;
  const float INV_TWO_PI = 0.159154943f;

  x = x - TWO_PI * (int)(x * INV_TWO_PI);
  if (x < 0)
    x += TWO_PI;

  // Convert to table index
  float idx_f = x * (FAST_SIN_TABLE_SIZE / TWO_PI);
  int idx = (int)idx_f;
  float frac = idx_f - idx;

  // Linear interpolation
  int idx_next = (idx + 1) & FAST_SIN_TABLE_MASK;
  return fast_sin_table[idx & FAST_SIN_TABLE_MASK] * (1.0f - frac) +
         fast_sin_table[idx_next] * frac;
}

/**
 * Fast cosine using LUT.
 */
static inline float fast_cosf(float x) {
  const float HALF_PI = 1.5707963268f;
  return fast_sinf(x + HALF_PI);
}

// ============================================================================
// Fast Square Root (Quake-style + Newton-Raphson)
// ============================================================================

static inline float fast_sqrtf_approx(float x) {
  if (x <= 0)
    return 0;

  // Initial approximation using bit manipulation
  union {
    float f;
    uint32_t i;
  } u = {x};
  u.i = (1 << 29) + (u.i >> 1) - (1 << 22);

  // One Newton-Raphson iteration
  u.f = 0.5f * (u.f + x / u.f);

  return u.f;
}

// ============================================================================
// Fast Inverse Square Root (for normalization)
// ============================================================================

static inline float fast_rsqrtf(float x) {
  union {
    float f;
    uint32_t i;
  } u = {x};
  u.i = 0x5f3759df - (u.i >> 1);      // Magic number
  u.f *= 1.5f - 0.5f * x * u.f * u.f; // Newton-Raphson
  return u.f;
}

// ============================================================================
// Fast atan2 Approximation
// ============================================================================

/**
 * Fast atan2 approximation.
 * Accuracy: ~0.01 radians
 */
static inline float fast_atan2f(float y, float x) {
  const float PI = 3.14159265359f;
  const float HALF_PI = 1.5707963268f;

  float abs_x = x < 0 ? -x : x;
  float abs_y = y < 0 ? -y : y;

  // Compute atan in first octant
  float a = abs_x < abs_y ? abs_x / (abs_y + 1e-10f) : abs_y / (abs_x + 1e-10f);
  float s = a * a;

  // Polynomial approximation
  float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;

  // Map to correct quadrant
  if (abs_x < abs_y)
    r = HALF_PI - r;
  if (x < 0)
    r = PI - r;
  if (y < 0)
    r = -r;

  return r;
}

// ============================================================================
// SIMD-Friendly 4-Channel Batch Processing
// ============================================================================

#ifdef __SSE__
#include <xmmintrin.h>

/**
 * Process 4 samples in parallel using SSE.
 * For batch processing of mono channels.
 */
typedef struct {
  __m128 data; // 4 x float
} Vec4f;

static inline Vec4f vec4f_load(const float *ptr) {
  Vec4f v;
  v.data = _mm_loadu_ps(ptr);
  return v;
}

static inline void vec4f_store(float *ptr, Vec4f v) {
  _mm_storeu_ps(ptr, v.data);
}

static inline Vec4f vec4f_add(Vec4f a, Vec4f b) {
  Vec4f v;
  v.data = _mm_add_ps(a.data, b.data);
  return v;
}

static inline Vec4f vec4f_mul(Vec4f a, Vec4f b) {
  Vec4f v;
  v.data = _mm_mul_ps(a.data, b.data);
  return v;
}

static inline Vec4f vec4f_set1(float x) {
  Vec4f v;
  v.data = _mm_set1_ps(x);
  return v;
}

#endif // __SSE__

// ============================================================================
// ARM NEON Support (for STM32F4 with NEON or Cortex-A)
// ============================================================================

#ifdef __ARM_NEON
#include <arm_neon.h>

typedef struct {
  float32x4_t data;
} Vec4f_Neon;

// Similar implementations for NEON...
#endif

#ifdef __cplusplus
}
#endif

#endif // FAST_MATH_H
