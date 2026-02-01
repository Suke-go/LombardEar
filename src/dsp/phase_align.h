/**
 * @file phase_align.h
 * @brief GCC-PHAT based multi-channel phase alignment
 *
 * Estimates and corrects inter-channel timing differences caused by
 * wireless transmission jitter or clock drift.
 */

#ifndef PHASE_ALIGN_H
#define PHASE_ALIGN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PhaseAligner PhaseAligner;

/**
 * Create a phase aligner instance.
 *
 * @param fft_size    FFT size for cross-correlation (e.g., 256, 512, 1024)
 * @param sample_rate Sample rate in Hz
 * @param num_channels Number of channels to align (including reference)
 * @return Pointer to PhaseAligner or NULL on failure
 */
PhaseAligner *phase_align_create(int fft_size, int sample_rate,
                                 int num_channels);

/**
 * Destroy a phase aligner and free resources.
 */
void phase_align_destroy(PhaseAligner *pa);

/**
 * Reset internal state (running averages, etc.)
 */
void phase_align_reset(PhaseAligner *pa);

/**
 * Estimate phase offsets of all channels relative to the reference (ch 0).
 *
 * Uses GCC-PHAT (Generalized Cross-Correlation with Phase Transform).
 *
 * @param pa          PhaseAligner instance
 * @param channels    Array of channel pointers [ch0, ch1, ch2, ...]
 *                    ch0 is the reference channel
 * @param num_samples Number of samples per channel (should be <= fft_size)
 * @param offsets_samples Output: offset in samples for each non-reference
 * channel Array size should be (num_channels - 1) Positive = channel is delayed
 * relative to reference
 */
void phase_align_estimate(PhaseAligner *pa, const float *const *channels,
                          int num_samples, float *offsets_samples);

/**
 * Apply phase correction to a single channel using fractional delay.
 *
 * Uses linear interpolation for sub-sample accuracy.
 *
 * @param in          Input samples
 * @param out         Output buffer (can be same as in for in-place)
 * @param num_samples Number of samples
 * @param offset_samples Delay to apply (positive = delay, negative = advance)
 */
void phase_align_correct(const float *in, float *out, int num_samples,
                         float offset_samples);

/**
 * Get the last estimated offsets.
 * @param pa          PhaseAligner instance
 * @param offsets     Output array of size (num_channels - 1)
 */
void phase_align_get_offsets(const PhaseAligner *pa, float *offsets);

/**
 * Set smoothing factor for offset estimation (default: 0.1).
 * Lower values = slower tracking, more stable.
 */
void phase_align_set_smoothing(PhaseAligner *pa, float alpha);

#ifdef __cplusplus
}
#endif

#endif // PHASE_ALIGN_H
