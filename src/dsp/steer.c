#include "steer.h"
#include "doa.h"
#include "multiband.h"


float spatial_spectral_process(float xTL, float xTR, float xBL, float xBR,
                               float theta_deg, MultibandState *mb) {
  // Step 1: Spatial filtering (direction selection)
  float beam = steer_beam(xTL, xTR, xBL, xBR, theta_deg);

  // Step 2: Spectral shaping (frequency band control)
  float out = multiband_process(mb, beam);

  return out;
}

/**
 * Auto-tracking processor: DOA + Beamforming + Multiband in one call.
 * Automatically tracks the dominant sound source direction.
 */
float auto_track_process(float xTL, float xTR, float xBL, float xBR,
                         DoaState *doa, MultibandState *mb, float *out_theta,
                         float *out_confidence) {
  // Step 1: Update DOA estimate
  float theta = doa_update(doa, xTL, xTR, xBL, xBR);

  // Step 2: Spatial filtering with estimated direction
  float beam = steer_beam(xTL, xTR, xBL, xBR, theta);

  // Step 3: Spectral shaping
  float out = multiband_process(mb, beam);

  // Output tracking info
  if (out_theta)
    *out_theta = theta;
  if (out_confidence)
    *out_confidence = doa_get_confidence(doa);

  return out;
}
