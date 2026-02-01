/**
 * @file phase_align.c
 * @brief GCC-PHAT phase alignment implementation
 *
 * Uses a simple radix-2 FFT for cross-correlation computation.
 * Optimized for low-latency real-time processing.
 */

#include "phase_align.h"
#include "fast_math.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Maximum supported FFT size
#define MAX_FFT_SIZE 2048

struct PhaseAligner {
  int fft_size;
  int sample_rate;
  int num_channels;

  // FFT buffers (real/imag interleaved)
  float *fft_ref;     // Reference channel FFT
  float *fft_target;  // Target channel FFT
  float *fft_scratch; // Scratch buffer for IFFT result

  // Smoothed offset estimates
  float *offsets;
  float alpha; // Smoothing factor

  // Twiddle factors (precomputed)
  float *twiddle_real;
  float *twiddle_imag;
};

// ============================================================================
// Simple in-place radix-2 FFT (Cooley-Tukey)
// ============================================================================

static void fft_init_twiddle(float *real, float *imag, int n) {
  for (int i = 0; i < n / 2; i++) {
    float angle = -2.0f * (float)M_PI * (float)i / (float)n;
    real[i] = fast_cosf(angle);
    imag[i] = fast_sinf(angle);
  }
}

static int bit_reverse(int x, int bits) {
  int result = 0;
  for (int i = 0; i < bits; i++) {
    result = (result << 1) | (x & 1);
    x >>= 1;
  }
  return result;
}

static void fft_reorder(float *data_r, float *data_i, int n) {
  int bits = 0;
  int temp = n;
  while (temp > 1) {
    bits++;
    temp >>= 1;
  }

  for (int i = 0; i < n; i++) {
    int j = bit_reverse(i, bits);
    if (j > i) {
      float tr = data_r[i];
      float ti = data_i[i];
      data_r[i] = data_r[j];
      data_i[i] = data_i[j];
      data_r[j] = tr;
      data_i[j] = ti;
    }
  }
}

static void fft_compute(float *data_r, float *data_i, int n, const float *tw_r,
                        const float *tw_i, int inverse) {
  fft_reorder(data_r, data_i, n);

  for (int size = 2; size <= n; size *= 2) {
    int half = size / 2;
    int step = n / size;

    for (int i = 0; i < n; i += size) {
      for (int k = 0; k < half; k++) {
        int tw_idx = k * step;
        float wr = tw_r[tw_idx];
        float wi = inverse ? -tw_i[tw_idx] : tw_i[tw_idx];

        int idx0 = i + k;
        int idx1 = i + k + half;

        float tr = data_r[idx1] * wr - data_i[idx1] * wi;
        float ti = data_r[idx1] * wi + data_i[idx1] * wr;

        data_r[idx1] = data_r[idx0] - tr;
        data_i[idx1] = data_i[idx0] - ti;
        data_r[idx0] = data_r[idx0] + tr;
        data_i[idx0] = data_i[idx0] + ti;
      }
    }
  }

  // Normalize for inverse FFT
  if (inverse) {
    float scale = 1.0f / (float)n;
    for (int i = 0; i < n; i++) {
      data_r[i] *= scale;
      data_i[i] *= scale;
    }
  }
}

// ============================================================================
// Phase Aligner Implementation
// ============================================================================

PhaseAligner *phase_align_create(int fft_size, int sample_rate,
                                 int num_channels) {
  // Validate FFT size (must be power of 2)
  if (fft_size <= 0 || fft_size > MAX_FFT_SIZE ||
      (fft_size & (fft_size - 1)) != 0) {
    return NULL;
  }
  if (num_channels < 2) {
    return NULL;
  }

  PhaseAligner *pa = (PhaseAligner *)calloc(1, sizeof(PhaseAligner));
  if (!pa)
    return NULL;

  pa->fft_size = fft_size;
  pa->sample_rate = sample_rate;
  pa->num_channels = num_channels;
  pa->alpha = 0.1f;

  // Allocate buffers
  pa->fft_ref = (float *)calloc(fft_size * 2, sizeof(float)); // real + imag
  pa->fft_target = (float *)calloc(fft_size * 2, sizeof(float));
  pa->fft_scratch = (float *)calloc(fft_size * 2, sizeof(float));
  pa->offsets = (float *)calloc(num_channels - 1, sizeof(float));
  pa->twiddle_real = (float *)calloc(fft_size / 2, sizeof(float));
  pa->twiddle_imag = (float *)calloc(fft_size / 2, sizeof(float));

  if (!pa->fft_ref || !pa->fft_target || !pa->fft_scratch || !pa->offsets ||
      !pa->twiddle_real || !pa->twiddle_imag) {
    phase_align_destroy(pa);
    return NULL;
  }

  // Initialize twiddle factors
  fft_init_twiddle(pa->twiddle_real, pa->twiddle_imag, fft_size);

  return pa;
}

void phase_align_destroy(PhaseAligner *pa) {
  if (pa) {
    free(pa->fft_ref);
    free(pa->fft_target);
    free(pa->fft_scratch);
    free(pa->offsets);
    free(pa->twiddle_real);
    free(pa->twiddle_imag);
    free(pa);
  }
}

void phase_align_reset(PhaseAligner *pa) {
  if (pa && pa->offsets) {
    memset(pa->offsets, 0, (pa->num_channels - 1) * sizeof(float));
  }
}

void phase_align_estimate(PhaseAligner *pa, const float *const *channels,
                          int num_samples, float *offsets_samples) {
  if (!pa || !channels || !offsets_samples || num_samples <= 0)
    return;

  int n = pa->fft_size;
  int samples_to_use = (num_samples < n) ? num_samples : n;

  // Separate real/imag arrays for FFT
  float *ref_r = pa->fft_ref;
  float *ref_i = pa->fft_ref + n;
  float *tgt_r = pa->fft_target;
  float *tgt_i = pa->fft_target + n;
  float *corr_r = pa->fft_scratch;
  float *corr_i = pa->fft_scratch + n;

  // Copy and zero-pad reference channel
  memset(ref_r, 0, n * sizeof(float));
  memset(ref_i, 0, n * sizeof(float));
  memcpy(ref_r, channels[0], samples_to_use * sizeof(float));

  // FFT of reference
  fft_compute(ref_r, ref_i, n, pa->twiddle_real, pa->twiddle_imag, 0);

  // Process each target channel
  for (int ch = 1; ch < pa->num_channels; ch++) {
    // Copy and zero-pad target channel
    memset(tgt_r, 0, n * sizeof(float));
    memset(tgt_i, 0, n * sizeof(float));
    memcpy(tgt_r, channels[ch], samples_to_use * sizeof(float));

    // FFT of target
    fft_compute(tgt_r, tgt_i, n, pa->twiddle_real, pa->twiddle_imag, 0);

    // GCC-PHAT: G(f) = R(f) * conj(T(f)) / |R * conj(T)|
    for (int i = 0; i < n; i++) {
      // Cross-spectrum: R * conj(T)
      float xr = ref_r[i] * tgt_r[i] + ref_i[i] * tgt_i[i];
      float xi = ref_i[i] * tgt_r[i] - ref_r[i] * tgt_i[i];

      // Magnitude for PHAT normalization
      float mag = sqrtf(xr * xr + xi * xi) + 1e-10f;

      // Normalize (PHAT weighting)
      corr_r[i] = xr / mag;
      corr_i[i] = xi / mag;
    }

    // IFFT to get cross-correlation
    fft_compute(corr_r, corr_i, n, pa->twiddle_real, pa->twiddle_imag, 1);

    // Find peak in correlation
    float max_val = corr_r[0];
    int max_idx = 0;
    for (int i = 1; i < n; i++) {
      if (corr_r[i] > max_val) {
        max_val = corr_r[i];
        max_idx = i;
      }
    }

    // Convert index to lag (handle wrap-around)
    float lag = (float)max_idx;
    if (max_idx > n / 2) {
      lag = (float)(max_idx - n);
    }

    // Parabolic interpolation for sub-sample accuracy
    if (max_idx > 0 && max_idx < n - 1) {
      float y0 = corr_r[(max_idx - 1 + n) % n];
      float y1 = corr_r[max_idx];
      float y2 = corr_r[(max_idx + 1) % n];

      float delta = 0.5f * (y0 - y2) / (y0 - 2.0f * y1 + y2 + 1e-10f);
      if (delta > -1.0f && delta < 1.0f) {
        lag += delta;
      }
    }

    // Smooth the estimate
    int idx = ch - 1;
    pa->offsets[idx] = (1.0f - pa->alpha) * pa->offsets[idx] + pa->alpha * lag;
    offsets_samples[idx] = pa->offsets[idx];
  }
}

void phase_align_correct(const float *in, float *out, int num_samples,
                         float offset_samples) {
  if (!in || !out || num_samples <= 0)
    return;

  // Handle zero offset (just copy)
  if (fabsf(offset_samples) < 0.001f) {
    if (in != out) {
      memcpy(out, in, num_samples * sizeof(float));
    }
    return;
  }

  // Integer and fractional parts
  int int_delay = (int)floorf(offset_samples);
  float frac = offset_samples - (float)int_delay;

  // Linear interpolation coefficients
  float c0 = 1.0f - frac;
  float c1 = frac;

  // Apply delay with linear interpolation
  for (int i = 0; i < num_samples; i++) {
    int idx0 = i - int_delay;
    int idx1 = idx0 - 1;

    float s0 = (idx0 >= 0 && idx0 < num_samples) ? in[idx0] : 0.0f;
    float s1 = (idx1 >= 0 && idx1 < num_samples) ? in[idx1] : 0.0f;

    out[i] = c0 * s0 + c1 * s1;
  }
}

void phase_align_get_offsets(const PhaseAligner *pa, float *offsets) {
  if (pa && offsets) {
    memcpy(offsets, pa->offsets, (pa->num_channels - 1) * sizeof(float));
  }
}

void phase_align_set_smoothing(PhaseAligner *pa, float alpha) {
  if (pa && alpha > 0.0f && alpha <= 1.0f) {
    pa->alpha = alpha;
  }
}
