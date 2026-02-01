#include "multiband.h"

void multiband_init(MultibandState *st, float sample_rate) {
  // Crossover frequencies: 300Hz, 1000Hz, 4000Hz
  biquad_lowpass(&st->lp1, sample_rate, 300.0f);
  biquad_highpass(&st->hp1, sample_rate, 300.0f);
  biquad_lowpass(&st->lp2, sample_rate, 1000.0f);
  biquad_highpass(&st->hp2, sample_rate, 1000.0f);
  biquad_lowpass(&st->lp3, sample_rate, 4000.0f);
  biquad_highpass(&st->hp3, sample_rate, 4000.0f);

  // Default: voice enhancement preset
  multiband_preset_voice_enhance(st);
}

void multiband_set_gain(MultibandState *st, int band, float gain) {
  if (band >= 0 && band < MULTIBAND_NUM_BANDS) {
    st->gains[band] = gain;
  }
}

void multiband_set_gains(MultibandState *st, float g_low, float g_voice_low,
                         float g_voice_high, float g_high) {
  st->gains[BAND_LOW] = g_low;
  st->gains[BAND_VOICE_LOW] = g_voice_low;
  st->gains[BAND_VOICE_HIGH] = g_voice_high;
  st->gains[BAND_HIGH] = g_high;
}

float multiband_process(MultibandState *st, float in) {
  // Band splitting using Linkwitz-Riley style (LP + HP = original)
  // Band 0: Low (< 300 Hz)
  float low = biquad_process(&st->lp1, in);

  // Remaining signal (> 300 Hz)
  float mid_high = biquad_process(&st->hp1, in);

  // Band 1: Voice Low (300-1000 Hz)
  float voice_low = biquad_process(&st->lp2, mid_high);

  // Remaining (> 1000 Hz)
  float high_part = biquad_process(&st->hp2, mid_high);

  // Band 2: Voice High (1000-4000 Hz)
  float voice_high = biquad_process(&st->lp3, high_part);

  // Band 3: High (> 4000 Hz)
  float high = biquad_process(&st->hp3, high_part);

  // Apply gains and sum
  float out =
      low * st->gains[BAND_LOW] + voice_low * st->gains[BAND_VOICE_LOW] +
      voice_high * st->gains[BAND_VOICE_HIGH] + high * st->gains[BAND_HIGH];

  return out;
}

void multiband_preset_voice_enhance(MultibandState *st) {
  // Boost voice frequencies, cut environmental noise
  st->gains[BAND_LOW] = 0.5f;        // Reduce rumble
  st->gains[BAND_VOICE_LOW] = 1.0f;  // Keep fundamental
  st->gains[BAND_VOICE_HIGH] = 1.5f; // Boost consonants (clarity)
  st->gains[BAND_HIGH] = 0.7f;       // Reduce hiss
}

void multiband_preset_flat(MultibandState *st) {
  for (int i = 0; i < MULTIBAND_NUM_BANDS; i++) {
    st->gains[i] = 1.0f;
  }
}

void multiband_preset_noise_reduce(MultibandState *st) {
  st->gains[BAND_LOW] = 0.3f;
  st->gains[BAND_VOICE_LOW] = 0.8f;
  st->gains[BAND_VOICE_HIGH] = 1.0f;
  st->gains[BAND_HIGH] = 0.3f;
}
