#ifndef DOA_H
#define DOA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/**
 * DOA (Direction of Arrival) Estimator
 *
 * Uses energy-based comparison for ultra-low latency estimation.
 * O(1) per sample, suitable for real-time tracking.
 */

typedef struct {
  // Smoothed energy accumulators for each direction
  float e_front;
  float e_back;
  float e_left;
  float e_right;

  // Smoothing factor (0.01 = slow, 0.1 = fast)
  float alpha;

  // Current estimated angle
  float theta_deg;

  // Confidence (0-1)
  float confidence;

  // Tracking parameters
  float slew_rate;    // Max degrees per sample
  float target_theta; // Target angle before smoothing
} DoaState;

/**
 * Initialize DOA estimator.
 * @param alpha  Smoothing factor (typical: 0.01-0.05)
 * @param slew_rate  Max angular change per sample (degrees)
 */
void doa_init(DoaState *st, float alpha, float slew_rate);

/**
 * Reset DOA state.
 */
void doa_reset(DoaState *st);

/**
 * Update DOA estimate with new samples.
 * @param xTL, xTR, xBL, xBR  4-channel mic inputs
 * @return  Estimated angle (0-360 degrees, 0=front)
 */
float doa_update(DoaState *st, float xTL, float xTR, float xBL, float xBR);

/**
 * Get current estimated angle.
 */
static inline float doa_get_angle(const DoaState *st) { return st->theta_deg; }

/**
 * Get confidence level (0-1).
 * Low when energy is similar in all directions.
 */
static inline float doa_get_confidence(const DoaState *st) {
  return st->confidence;
}

#ifdef __cplusplus
}
#endif

#endif // DOA_H
