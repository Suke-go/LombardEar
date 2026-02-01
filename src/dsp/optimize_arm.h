#ifndef OPTIMIZE_ARM_H
#define OPTIMIZE_ARM_H

/**
 * ARM Cortex-M4 Optimizations
 *
 * Leverages:
 * - CMSIS-DSP library (when available)
 * - DSP instructions (SMLAD, SMUAD, etc.)
 * - Barrel shifter
 * - FPU (when present)
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// ============================================================================
// Platform Detection
// ============================================================================

#if defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_MAIN__)
#define ARM_CORTEX_M4_OR_HIGHER 1
#endif

#if defined(__ARM_FP)
#define ARM_FPU_PRESENT 1
#endif

// ============================================================================
// CMSIS-DSP Integration (optional)
// ============================================================================

#ifdef USE_CMSIS_DSP
#include "arm_math.h"

// Use CMSIS-DSP types
typedef q15_t cmsis_q15_t;
typedef q31_t cmsis_q31_t;
typedef float32_t cmsis_float_t;
#endif

// ============================================================================
// Optimized Dot Product (4-channel beamformer core)
// ============================================================================

#ifdef ARM_CORTEX_M4_OR_HIGHER

// SIMD-accelerated dot product (4 elements)
static inline int32_t dot4_q15_simd(const int16_t *a, const int16_t *b) {
  int32_t result;

// Use SMLAD instruction: signed multiply-accumulate dual
// Processes 2 elements at once
#ifdef __GNUC__
  __asm__ volatile(
      "ldr r0, [%[a], #0]\n\t"       // Load a[0:1]
      "ldr r1, [%[b], #0]\n\t"       // Load b[0:1]
      "smuad r2, r0, r1\n\t"         // r2 = a[0]*b[0] + a[1]*b[1]
      "ldr r0, [%[a], #4]\n\t"       // Load a[2:3]
      "ldr r1, [%[b], #4]\n\t"       // Load b[2:3]
      "smlad %[out], r0, r1, r2\n\t" // out = a[2]*b[2] + a[3]*b[3] + r2
      : [out] "=r"(result)
      : [a] "r"(a), [b] "r"(b)
      : "r0", "r1", "r2");
#else
  // Fallback
  result = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
#endif

  return result;
}

#else

// Non-ARM fallback
static inline int32_t dot4_q15_simd(const int16_t *a, const int16_t *b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

#endif

// ============================================================================
// Fast Clamp (Compiler Hint)
// ============================================================================

static inline float clamp_fast(float x, float lo, float hi) {
#ifdef __GNUC__
  return __builtin_fmaxf(__builtin_fminf(x, hi), lo);
#else
  if (x < lo)
    return lo;
  if (x > hi)
    return hi;
  return x;
#endif
}

// ============================================================================
// Prefetch Hints for Cache Optimization
// ============================================================================

static inline void prefetch_read(const void *ptr) {
#ifdef __GNUC__
  __builtin_prefetch(ptr, 0, 3); // 0=read, 3=high locality
#endif
}

static inline void prefetch_write(void *ptr) {
#ifdef __GNUC__
  __builtin_prefetch(ptr, 1, 3); // 1=write, 3=high locality
#endif
}

// ============================================================================
// Memory Barrier (for DMA/interrupt safety)
// ============================================================================

static inline void mem_barrier(void) {
#ifdef __GNUC__
  __asm__ volatile("dmb" ::: "memory");
#endif
}

// ============================================================================
// Cycle Counter (for profiling on Cortex-M)
// ============================================================================

#ifdef ARM_CORTEX_M4_OR_HIGHER

#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)
#define DWT_CTRL (*(volatile uint32_t *)0xE0001000)
#define SCB_DEMCR (*(volatile uint32_t *)0xE000EDFC)

static inline void cycle_counter_enable(void) {
  SCB_DEMCR |= 0x01000000; // Enable TRCENA
  DWT_CYCCNT = 0;
  DWT_CTRL |= 1; // Enable cycle counter
}

static inline uint32_t cycle_counter_read(void) { return DWT_CYCCNT; }

#else

// Desktop fallback (no-op)
static inline void cycle_counter_enable(void) {}
static inline uint32_t cycle_counter_read(void) { return 0; }

#endif

#ifdef __cplusplus
}
#endif

#endif // OPTIMIZE_ARM_H
