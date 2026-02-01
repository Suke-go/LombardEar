#ifndef MULTIBAND_H
#define MULTIBAND_H

#include "biquad.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MULTIBAND_NUM_BANDS 4

// Band indices
#define BAND_LOW 0        // < 300 Hz
#define BAND_VOICE_LOW 1  // 300-1000 Hz
#define BAND_VOICE_HIGH 2 // 1000-4000 Hz (voice clarity)
#define BAND_HIGH 3       // > 4000 Hz

typedef struct {
  // Crossover filters (LP/HP pairs for band splitting)
  BiquadState lp1; // 300 Hz lowpass
  BiquadState hp1; // 300 Hz highpass
  BiquadState lp2; // 1000 Hz lowpass
  BiquadState hp2; // 1000 Hz highpass
  BiquadState lp3; // 4000 Hz lowpass
  BiquadState hp3; // 4000 Hz highpass

  // Per-band gains (linear, 0-2.0)
  float gains[MULTIBAND_NUM_BANDS];
} MultibandState;

// Initialize with default voice-optimized gains
void multiband_init(MultibandState *st, float sample_rate);

// Set individual band gain (0.0-2.0, 1.0 = unity)
void multiband_set_gain(MultibandState *st, int band, float gain);

// Set all gains at once
void multiband_set_gains(MultibandState *st, float g_low, float g_voice_low,
                         float g_voice_high, float g_high);

// Process one sample (O(1), ~30 cycles)
float multiband_process(MultibandState *st, float in);

// Preset configurations
void multiband_preset_voice_enhance(MultibandState *st); // Boost voice bands
void multiband_preset_flat(MultibandState *st);          // All unity
void multiband_preset_noise_reduce(MultibandState *st);  // Cut low/high

#ifdef __cplusplus
}
#endif

#endif // MULTIBAND_H
