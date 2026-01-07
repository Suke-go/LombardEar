#include "noise_gate.h"
#include <math.h>

void noise_gate_init(NoiseGateState *st, float threshold_db, float hold_ms,
                     float release_ms, int sample_rate) {
  if (!st)
    return;

  st->threshold_linear = powf(10.0f, threshold_db / 20.0f);
  st->hold_time_samples = (hold_ms / 1000.0f) * sample_rate;

  if (release_ms < 0.1f)
    release_ms = 0.1f;
  st->release_coeff = expf(-1.0f / (release_ms * 0.001f * sample_rate));

  st->envelope = 0.0f;
  st->gain = 0.0f; // Start muted
  st->hold_counter = 0;
  st->sample_rate = sample_rate;
}

float noise_gate_process(NoiseGateState *st, float in) {
  float abs_in = fabsf(in);

  // Fast Attack Envelope
  if (abs_in > st->envelope) {
    st->envelope = abs_in; // Instant attack for gate opening
  } else {
    // Smoother envelope decay? Or standard decay
    st->envelope = 0.99f * st->envelope + 0.01f * abs_in;
  }

  if (st->envelope > st->threshold_linear) {
    // Open Gate
    st->gain = 1.0f;
    st->hold_counter = (int)st->hold_time_samples;
  } else {
    // Below threshold
    if (st->hold_counter > 0) {
      // Holding open
      st->hold_counter--;
      st->gain = 1.0f;
    } else {
      // Release (fade out)
      st->gain = st->release_coeff * st->gain;
      // If gain is negligible, snap to 0 to save CPU/avoid denormals?
      if (st->gain < 1e-5f)
        st->gain = 0.0f;
    }
  }

  return in * st->gain;
}
