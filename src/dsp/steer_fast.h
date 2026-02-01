#ifndef STEER_FAST_H
#define STEER_FAST_H

/**
 * High-Performance Steerable Beamformer
 *
 * Optimizations:
 * - LUT-based sin/cos for steering weights
 * - Batch processing (N samples at once)
 * - SIMD-accelerated where available
 * - Cache-friendly memory layout
 */

#include "doa.h"
#include "fast_math.h"
#include "multiband.h"


#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Batch Processing Structures
// ============================================================================

#define BATCH_SIZE 64 // Process 64 samples at once (4ms @ 16kHz)

typedef struct {
  float xTL[BATCH_SIZE];
  float xTR[BATCH_SIZE];
  float xBL[BATCH_SIZE];
  float xBR[BATCH_SIZE];
} Mic4Batch;

typedef struct {
  float out[BATCH_SIZE];
} OutputBatch;

// ============================================================================
// Pre-computed Steering Weights (LUT for 360 angles)
// ============================================================================

#define STEER_ANGLE_STEPS 360

typedef struct {
  float w_front[STEER_ANGLE_STEPS];
  float w_back[STEER_ANGLE_STEPS];
  float w_left[STEER_ANGLE_STEPS];
  float w_right[STEER_ANGLE_STEPS];
  float w_sum_inv[STEER_ANGLE_STEPS]; // 1/sum for normalization
} SteerWeightLUT;

// Global LUT (initialized once)
extern SteerWeightLUT g_steer_lut;

// Initialize steering LUT
void steer_lut_init(void);

// ============================================================================
// Fast Single-Sample Processing
// ============================================================================

/**
 * Ultra-fast beam steering using pre-computed LUT.
 * O(1) with only table lookup + 4 MACs.
 *
 * @param theta_idx  Angle index (0-359)
 */
static inline float steer_beam_fast(float xTL, float xTR, float xBL, float xBR,
                                    int theta_idx) {
  theta_idx = theta_idx & 0xFF; // Wrap to 0-255 (fast modulo for power of 2)
  if (theta_idx >= STEER_ANGLE_STEPS)
    theta_idx = 0;

  float beam_front = 0.5f * (xTL + xTR);
  float beam_back = 0.5f * (xBL + xBR);
  float beam_left = 0.5f * (xTL + xBL);
  float beam_right = 0.5f * (xTR + xBR);

  float out = g_steer_lut.w_front[theta_idx] * beam_front +
              g_steer_lut.w_back[theta_idx] * beam_back +
              g_steer_lut.w_left[theta_idx] * beam_left +
              g_steer_lut.w_right[theta_idx] * beam_right;

  return out * g_steer_lut.w_sum_inv[theta_idx];
}

/**
 * Convert degrees to LUT index.
 */
static inline int steer_deg_to_idx(float theta_deg) {
  int idx = (int)(theta_deg + 0.5f);
  if (idx < 0)
    idx += 360;
  if (idx >= 360)
    idx -= 360;
  return idx;
}

// ============================================================================
// Batch Processing Functions
// ============================================================================

/**
 * Process batch of samples with fixed steering angle.
 * Highly optimized for cache efficiency.
 */
void steer_batch_process(const Mic4Batch *in, OutputBatch *out, int theta_idx,
                         int count);

/**
 * Process batch with auto-tracking DOA.
 * Updates DOA state and steers beam to estimated direction.
 */
void steer_batch_auto_track(const Mic4Batch *in, OutputBatch *out,
                            DoaState *doa, int count);

/**
 * Full spatial-spectral batch processing.
 * Steering + Multiband EQ in one optimized pass.
 */
void spatial_spectral_batch(const Mic4Batch *in, OutputBatch *out,
                            int theta_idx, MultibandState *mb, int count);

// ============================================================================
// Performance Metrics
// ============================================================================

typedef struct {
  uint32_t samples_processed;
  uint32_t total_cycles;
  float avg_cycles_per_sample;
} PerfMetrics;

void perf_reset(PerfMetrics *m);
void perf_update(PerfMetrics *m, uint32_t samples, uint32_t cycles);

#ifdef __cplusplus
}
#endif

#endif // STEER_FAST_H
