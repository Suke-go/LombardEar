#ifndef NOISE_GATE_H
#define NOISE_GATE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float threshold_linear; // Threshold in linear scale
  float hold_time_samples;
  float release_coeff;
  float envelope;   // Current envelope
  float gain;       // Current gain
  int hold_counter; // Counter for hold time
  int sample_rate;
} NoiseGateState;

/**
 * Initialize Noise Gate.
 * @param st: State structure
 * @param threshold_db: Threshold in dB (e.g., -50.0f)
 * @param hold_ms: Hold time in ms (e.g., 200.0f)
 * @param release_ms: Release time in ms (e.g., 100.0f)
 * @param sample_rate: Sample rate in Hz
 */
void noise_gate_init(NoiseGateState *st, float threshold_db, float hold_ms,
                     float release_ms, int sample_rate);

/**
 * Process one sample.
 * @param st: State structure
 * @param in: Input sample
 * @return Processed sample (muted or original)
 */
float noise_gate_process(NoiseGateState *st, float in);

#ifdef __cplusplus
}
#endif

#endif // NOISE_GATE_H
