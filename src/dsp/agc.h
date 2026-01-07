#ifndef AGC_H
#define AGC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float target_rms;    // Target RMS level (linear)
  float max_gain;      // Maximum gain limit (linear)
  float envelope;      // Current envelope estimate
  float gain;          // Current gain
  float attack_coeff;  // Attack coefficient
  float release_coeff; // Release coefficient
  int hold_counter;    // Hold counter (samples)
  int hold_time;       // Hold time (samples)
} AgcState;

/**
 * Initialize AGC state.
 * @param target_db: Target level in dB (e.g., -20.0f)
 * @param attack_ms: Attack time in milliseconds (e.g., 10.0f)
 * @param release_ms: Release time in milliseconds (e.g., 200.0f)
 * @param max_gain_db: Maximum amplification in dB (e.g., 30.0f)
 * @param sample_rate: Sample rate in Hz
 */
void agc_init(AgcState *st, float target_db, float attack_ms, float release_ms,
              float max_gain_db, int sample_rate);

/**
 * Process one sample with AGC.
 * @param st: State structure
 * @param in: Input sample
 * @return Processed sample
 */
float agc_process(AgcState *st, float in);

#ifdef __cplusplus
}
#endif

#endif // AGC_H
