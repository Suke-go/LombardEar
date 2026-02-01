#include "biquad.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void biquad_init(BiquadState *st, float b0, float b1, float b2, float a1,
                 float a2) {
  st->b0 = b0;
  st->b1 = b1;
  st->b2 = b2;
  st->a1 = a1;
  st->a2 = a2;
  st->z1 = 0.0f;
  st->z2 = 0.0f;
}

void biquad_reset(BiquadState *st) {
  st->z1 = 0.0f;
  st->z2 = 0.0f;
}

void biquad_lowpass(BiquadState *st, float sample_rate, float cutoff_hz) {
  float w0 = 2.0f * M_PI * cutoff_hz / sample_rate;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha = sin_w0 / 1.4142135f; // Q = 1/sqrt(2) for Butterworth

  float a0 = 1.0f + alpha;
  st->b0 = (1.0f - cos_w0) / 2.0f / a0;
  st->b1 = (1.0f - cos_w0) / a0;
  st->b2 = (1.0f - cos_w0) / 2.0f / a0;
  st->a1 = -2.0f * cos_w0 / a0;
  st->a2 = (1.0f - alpha) / a0;
  st->z1 = 0.0f;
  st->z2 = 0.0f;
}

void biquad_highpass(BiquadState *st, float sample_rate, float cutoff_hz) {
  float w0 = 2.0f * M_PI * cutoff_hz / sample_rate;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float alpha = sin_w0 / 1.4142135f;

  float a0 = 1.0f + alpha;
  st->b0 = (1.0f + cos_w0) / 2.0f / a0;
  st->b1 = -(1.0f + cos_w0) / a0;
  st->b2 = (1.0f + cos_w0) / 2.0f / a0;
  st->a1 = -2.0f * cos_w0 / a0;
  st->a2 = (1.0f - alpha) / a0;
  st->z1 = 0.0f;
  st->z2 = 0.0f;
}

void biquad_bandpass(BiquadState *st, float sample_rate, float center_hz,
                     float bandwidth_hz) {
  float w0 = 2.0f * M_PI * center_hz / sample_rate;
  float cos_w0 = cosf(w0);
  float sin_w0 = sinf(w0);
  float Q = center_hz / bandwidth_hz;
  float alpha = sin_w0 / (2.0f * Q);

  float a0 = 1.0f + alpha;
  st->b0 = alpha / a0;
  st->b1 = 0.0f;
  st->b2 = -alpha / a0;
  st->a1 = -2.0f * cos_w0 / a0;
  st->a2 = (1.0f - alpha) / a0;
  st->z1 = 0.0f;
  st->z2 = 0.0f;
}
