/**
 * @file jitter_buffer.h
 * @brief Adaptive Jitter Buffer for wireless microphone synchronization
 *
 * Compensates for timing jitter in wireless audio links (e.g., Hollyland Lark
 * A1). Features:
 * - PLL-based clock drift tracking
 * - Adaptive buffer depth (target: 50-200ms)
 * - Linear interpolation for underrun recovery
 */

#ifndef JITTER_BUFFER_H
#define JITTER_BUFFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JitterBuffer JitterBuffer;

/**
 * Statistics for monitoring jitter buffer performance.
 */
typedef struct {
  float delay_ms;       // Current buffer delay in milliseconds
  float jitter_mean_ms; // Mean jitter (EWMA)
  float jitter_std_ms;  // Jitter standard deviation estimate
  float fill_ratio;     // Buffer fill ratio [0.0, 1.0]
  int underruns;        // Total underrun count since creation
  float drift_ppm;      // Estimated clock drift in PPM
} JitterStats;

/**
 * Create a new jitter buffer.
 *
 * @param sample_rate   Audio sample rate in Hz (e.g., 48000)
 * @param channels      Number of interleaved channels
 * @param target_delay_ms  Target buffering delay in milliseconds
 * @return Pointer to new JitterBuffer, or NULL on failure
 */
JitterBuffer *jitter_buffer_create(int sample_rate, int channels,
                                   int target_delay_ms);

/**
 * Destroy a jitter buffer and free resources.
 */
void jitter_buffer_destroy(JitterBuffer *jb);

/**
 * Reset the jitter buffer to initial state.
 */
void jitter_buffer_reset(JitterBuffer *jb);

/**
 * Write samples to the jitter buffer (producer/receive side).
 *
 * @param jb           Jitter buffer instance
 * @param samples      Interleaved sample data
 * @param num_samples  Number of sample frames (not total floats)
 * @param timestamp_us Packet timestamp in microseconds (optional, 0 if unknown)
 * @return Number of samples actually written (may be less if buffer full)
 */
int jitter_buffer_write(JitterBuffer *jb, const float *samples, int num_samples,
                        uint64_t timestamp_us);

/**
 * Read samples from the jitter buffer (consumer/DSP side).
 *
 * Uses linear interpolation if buffer underruns.
 *
 * @param jb           Jitter buffer instance
 * @param out          Output buffer for interleaved samples
 * @param num_samples  Number of sample frames to read
 * @return Number of samples actually read (may include interpolated data)
 */
int jitter_buffer_read(JitterBuffer *jb, float *out, int num_samples);

/**
 * Get current jitter buffer statistics.
 */
void jitter_buffer_get_stats(const JitterBuffer *jb, JitterStats *stats);

/**
 * Set target delay dynamically.
 * The buffer will gradually adjust to the new target.
 */
void jitter_buffer_set_target_delay(JitterBuffer *jb, int target_delay_ms);

#ifdef __cplusplus
}
#endif

#endif // JITTER_BUFFER_H
