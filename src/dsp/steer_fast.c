#include "steer_fast.h"
#include <math.h>

// Global steering weight LUT
SteerWeightLUT g_steer_lut;

void steer_lut_init(void) {
  const float PI = 3.14159265359f;

  for (int i = 0; i < STEER_ANGLE_STEPS; i++) {
    float theta = i * (PI / 180.0f);

    float cos_t = cosf(theta);
    float sin_t = sinf(theta);

    // Cardioid weights for each direction
    float w_front = cos_t > 0 ? cos_t : 0;
    float w_back = cos_t < 0 ? -cos_t : 0;
    float w_right = sin_t > 0 ? sin_t : 0;
    float w_left = sin_t < 0 ? -sin_t : 0;

    g_steer_lut.w_front[i] = w_front;
    g_steer_lut.w_back[i] = w_back;
    g_steer_lut.w_left[i] = w_left;
    g_steer_lut.w_right[i] = w_right;

    float w_sum = w_front + w_back + w_left + w_right;
    g_steer_lut.w_sum_inv[i] = w_sum > 1e-6f ? 1.0f / w_sum : 1.0f;
  }
}

void steer_batch_process(const Mic4Batch *in, OutputBatch *out, int theta_idx,
                         int count) {
  if (count > BATCH_SIZE)
    count = BATCH_SIZE;

  // Pre-fetch weights
  float wf = g_steer_lut.w_front[theta_idx];
  float wb = g_steer_lut.w_back[theta_idx];
  float wl = g_steer_lut.w_left[theta_idx];
  float wr = g_steer_lut.w_right[theta_idx];
  float ws_inv = g_steer_lut.w_sum_inv[theta_idx];

  // Batch process (compiler will auto-vectorize this loop)
  for (int i = 0; i < count; i++) {
    float xTL = in->xTL[i];
    float xTR = in->xTR[i];
    float xBL = in->xBL[i];
    float xBR = in->xBR[i];

    float beam_front = 0.5f * (xTL + xTR);
    float beam_back = 0.5f * (xBL + xBR);
    float beam_left = 0.5f * (xTL + xBL);
    float beam_right = 0.5f * (xTR + xBR);

    out->out[i] =
        (wf * beam_front + wb * beam_back + wl * beam_left + wr * beam_right) *
        ws_inv;
  }
}

void steer_batch_auto_track(const Mic4Batch *in, OutputBatch *out,
                            DoaState *doa, int count) {
  if (count > BATCH_SIZE)
    count = BATCH_SIZE;

  for (int i = 0; i < count; i++) {
    float xTL = in->xTL[i];
    float xTR = in->xTR[i];
    float xBL = in->xBL[i];
    float xBR = in->xBR[i];

    // Update DOA estimate
    float theta = doa_update(doa, xTL, xTR, xBL, xBR);
    int theta_idx = steer_deg_to_idx(theta);

    // Steer beam
    out->out[i] = steer_beam_fast(xTL, xTR, xBL, xBR, theta_idx);
  }
}

void spatial_spectral_batch(const Mic4Batch *in, OutputBatch *out,
                            int theta_idx, MultibandState *mb, int count) {
  if (count > BATCH_SIZE)
    count = BATCH_SIZE;

  // Step 1: Batch beamforming
  steer_batch_process(in, out, theta_idx, count);

  // Step 2: Apply multiband EQ to each sample
  for (int i = 0; i < count; i++) {
    out->out[i] = multiband_process(mb, out->out[i]);
  }
}

void perf_reset(PerfMetrics *m) {
  m->samples_processed = 0;
  m->total_cycles = 0;
  m->avg_cycles_per_sample = 0;
}

void perf_update(PerfMetrics *m, uint32_t samples, uint32_t cycles) {
  m->samples_processed += samples;
  m->total_cycles += cycles;
  if (m->samples_processed > 0) {
    m->avg_cycles_per_sample = (float)m->total_cycles / m->samples_processed;
  }
}
