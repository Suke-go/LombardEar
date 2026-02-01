#ifndef BIQUAD_H
#define BIQUAD_H

#ifdef __cplusplus
extern "C" {
#endif

// Biquad filter state (Direct Form II Transposed)
typedef struct {
  float b0, b1, b2; // Feedforward coefficients
  float a1, a2;     // Feedback coefficients (a0 normalized to 1)
  float z1, z2;     // State variables
} BiquadState;

// Initialize with pre-calculated coefficients
void biquad_init(BiquadState *st, float b0, float b1, float b2, float a1,
                 float a2);

// Reset state (clear history)
void biquad_reset(BiquadState *st);

// Process one sample (O(1), ~5 MACs)
static inline float biquad_process(BiquadState *st, float in) {
  float out = st->b0 * in + st->z1;
  st->z1 = st->b1 * in - st->a1 * out + st->z2;
  st->z2 = st->b2 * in - st->a2 * out;
  return out;
}

// Coefficient calculators for common filter types
// All designs use sample_rate and cutoff frequency in Hz

// Lowpass filter (2nd order Butterworth)
void biquad_lowpass(BiquadState *st, float sample_rate, float cutoff_hz);

// Highpass filter (2nd order Butterworth)
void biquad_highpass(BiquadState *st, float sample_rate, float cutoff_hz);

// Bandpass filter (constant peak gain)
void biquad_bandpass(BiquadState *st, float sample_rate, float center_hz,
                     float bandwidth_hz);

#ifdef __cplusplus
}
#endif

#endif // BIQUAD_H
