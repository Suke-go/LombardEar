#ifndef STEER_H
#define STEER_H

#include "multiband.h"
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Microphone geometry (in meters)
#define MIC_SPACING_LR 0.13f // 13cm left-right
#define MIC_SPACING_FB 0.15f // 15cm front-back

// Sound speed (m/s)
#define SOUND_SPEED 343.0f

/**
 * Steerable beamformer with continuous angle selection.
 *
 * @param xTL  Temple Left sample
 * @param xTR  Temple Right sample
 * @param xBL  Back Left sample
 * @param xBR  Back Right sample
 * @param theta_deg  Steering angle in degrees (0=front, 90=right, 180=back,
 * 270=left)
 * @return  Steered beam output
 */
static inline float steer_beam(float xTL, float xTR, float xBL, float xBR,
                               float theta_deg) {
  // Convert to radians
  float theta = theta_deg * (M_PI / 180.0f);

  // Direction vector components
  float cos_t = cosf(theta); // +1 = front, -1 = back
  float sin_t = sinf(theta); // +1 = right, -1 = left

  // Compute directional weights using cardioid pattern
  // Front: cos_t > 0, Back: cos_t < 0
  // Right: sin_t > 0, Left: sin_t < 0

  // Non-negative weights for each direction
  float w_front = fmaxf(0.0f, cos_t);
  float w_back = fmaxf(0.0f, -cos_t);
  float w_right = fmaxf(0.0f, sin_t);
  float w_left = fmaxf(0.0f, -sin_t);

  // Compute beam outputs for each direction pair
  float beam_front = 0.5f * (xTL + xTR);
  float beam_back = 0.5f * (xBL + xBR);
  float beam_left = 0.5f * (xTL + xBL);
  float beam_right = 0.5f * (xTR + xBR);

  // Weighted sum with normalization
  float w_sum = w_front + w_back + w_right + w_left;
  if (w_sum < 1e-6f)
    w_sum = 1.0f; // Avoid division by zero

  float out = (w_front * beam_front + w_back * beam_back + w_left * beam_left +
               w_right * beam_right) /
              w_sum;

  return out;
}

/**
 * Compute interference reference for GSC (opposite direction).
 */
static inline float steer_reference(float xTL, float xTR, float xBL, float xBR,
                                    float theta_deg) {
  // Reference is the opposite direction (180 degrees shifted)
  return steer_beam(xTL, xTR, xBL, xBR, theta_deg + 180.0f);
}

/**
 * Full spatial-spectral processor: beam + multiband in one call.
 */
float spatial_spectral_process(float xTL, float xTR, float xBL, float xBR,
                               float theta_deg, MultibandState *mb);

/**
 * Auto-tracking processor: DOA + Beamforming + Multiband.
 * Automatically tracks dominant sound source direction.
 *
 * @param doa  DOA estimator state
 * @param mb   Multiband processor state
 * @param out_theta  Optional output for current estimated angle
 * @param out_confidence  Optional output for tracking confidence
 */
#include "doa.h"
float auto_track_process(float xTL, float xTR, float xBL, float xBR,
                         DoaState *doa, MultibandState *mb, float *out_theta,
                         float *out_confidence);

#ifdef __cplusplus
}
#endif

#endif // STEER_H
