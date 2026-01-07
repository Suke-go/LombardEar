#include "aec.h"
#include <string.h>

int aec_init(AecState *st, int filter_len, float *mem, size_t mem_size) {
  if (!st || !mem || filter_len <= 0)
    return -1;

  // Requirement: 2 buffers of length M (w and x_history)
  size_t required_size = 2 * filter_len * sizeof(float);
  if (mem_size < required_size)
    return -1;

  memset(mem, 0, required_size);

  st->M = filter_len;
  st->w = mem;
  st->x_history = mem + filter_len;
  st->write_idx = 0;

  // Defaults
  st->mu = 0.05f;
  st->param_regularization = 1e-6f;
  st->power_est = 0.0f;

  return 0;
}

float aec_process(AecState *st, float mic_in, float ref_in) {
  // 1. Update reference history (Circular buffer)
  st->x_history[st->write_idx] = ref_in;

  // 2. Filter (Convolution)
  // y_est = w^T * x
  float y_est = 0.0f;

  int idx = st->write_idx;
  for (int i = 0; i < st->M; i++) {
    y_est += st->w[i] * st->x_history[idx];

    // Decrement circular index
    idx--;
    if (idx < 0)
      idx = st->M - 1;
  }

  // 3. Error calculation
  // e = d - y_est = mic_in - y_est
  float e = mic_in - y_est;

  // 4. Update Power Estimate (Simple L2 norm or Exponential Average)
  // For NLMS, we need ||x||^2
  // Optimization: Update iteratively? Or just recompute?
  // recomputing for stability for now, or use sliding window
  // subtraction/addition

  // Power of vector x_history.
  // Optimization: Running sum of squares is faster but risks drift.
  // Let's use loop for safety for now (M is small, e.g. 256).
  // AVX2 optimization would act here if critical.
  float x_norm_sq = 0.0f;
  for (int i = 0; i < st->M; i++) {
    x_norm_sq += st->x_history[i] * st->x_history[i];
  }

  // 5. Update Weights (NLMS)
  // w(n+1) = w(n) + mu * e(n) * x(n) / (||x(n)||^2 + eps)
  float step = (st->mu * e) / (x_norm_sq + st->param_regularization);

  idx = st->write_idx;
  for (int i = 0; i < st->M; i++) {
    st->w[i] += step * st->x_history[idx];

    idx--;
    if (idx < 0)
      idx = st->M - 1;
  }

  // Check for divergence? (Desirable for stability)

  // Increment global write index for next sample
  st->write_idx++;
  if (st->write_idx >= st->M)
    st->write_idx = 0;

  return e;
}

void aec_set_step_size(AecState *st, float mu) {
  if (st)
    st->mu = mu;
}
