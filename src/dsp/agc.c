#include "agc.h"
#include <math.h>

void agc_init(AgcState *st, float target_db, float attack_ms, float release_ms,
              float max_gain_db, int sample_rate) {
  if (!st)
    return;

  st->target_rms = powf(10.0f, target_db / 20.0f);
  st->max_gain = powf(10.0f, max_gain_db / 20.0f);
  st->envelope = 0.0f;
  st->gain = 1.0f;

  // Calculate coefficients
  // coeff = exp(-1 / (time_ms * fs / 1000))
  // approx: 1 - (1/(time*fs)) for fast approximation or use expf logic
  // Let's use expf for accuracy.
  float dt = 1.0f / (float)sample_rate;

  // Handling 0 or negative times safely
  if (attack_ms < 0.1f)
    attack_ms = 0.1f;
  if (release_ms < 0.1f)
    release_ms = 0.1f;

  st->attack_coeff = expf(-1.0f / (attack_ms * 0.001f * sample_rate));
  st->release_coeff = expf(-1.0f / (release_ms * 0.001f * sample_rate));
}

float agc_process(AgcState *st, float in) {
  float abs_in = fabsf(in);

  // 1. Envelope Detection
  if (abs_in > st->envelope) {
    // Attack phase (signal rising)
    st->envelope =
        st->attack_coeff * st->envelope + (1.0f - st->attack_coeff) * abs_in;
  } else {
    // Release phase (signal falling)
    st->envelope =
        st->release_coeff * st->envelope + (1.0f - st->release_coeff) * abs_in;
  }

  // Prevent division by zero
  if (st->envelope < 1e-9f)
    st->envelope = 1e-9f;

  // 2. Calculate Gain
  // Expected Gain = Target / Envelope
  float target_gain = st->target_rms / st->envelope;

  // 3. Limit Gain
  if (target_gain > st->max_gain)
    target_gain = st->max_gain;
  // Don't reduce gain below 0 (attenuation permitted?)
  // Usually AGC can attenuate if signal is too loud.
  // The logic "target_gain = target / env" implies attenuation if env > target.

  // Apply smoothing to gain changes for fewer artifacts?
  // Traditionally simpler AGCs just use the envelope to determine gain directly
  // or smoothed gain. Let's smooth the gain application itself slightly to
  // avoid zipper noise if envelope jumps. Reuse release/attack coeffs or
  // separate? Let's treat calculated gain as target. For simplicity here, we
  // use the detected envelope directly.

  st->gain = target_gain;

  // 4. Correct output
  return in * st->gain;
}
