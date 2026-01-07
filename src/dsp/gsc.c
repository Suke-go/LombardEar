#include "gsc.h"
#include "math_fast.h"
#include <immintrin.h>
#include <string.h>

static inline float clampf(float v, float min, float max) {
  if (v < min)
    return min;
  if (v > max)
    return max;
  return v;
}

int gsc_init(GscState *st, const GscConfig *cfg, void *mem, size_t mem_bytes) {
  if (!st || !cfg || !mem)
    return -1;

  size_t required = 4 * cfg->M * sizeof(float);
  if (mem_bytes < required)
    return -1;

  st->M = cfg->M;

  float *ptr = (float *)mem;
  st->w1 = ptr;
  ptr += cfg->M;
  st->w2 = ptr;
  ptr += cfg->M;
  st->u1_hist = ptr;
  ptr += cfg->M;
  st->u2_hist = ptr;
  ptr += cfg->M;

  gsc_reset(st);
  return 0;
}

void gsc_reset(GscState *st) {
  st->p_idx = 0;
  st->beta = 0.0f;

  memset(st->w1, 0, st->M * sizeof(float));
  memset(st->w2, 0, st->M * sizeof(float));
  memset(st->u1_hist, 0, st->M * sizeof(float));
  memset(st->u2_hist, 0, st->M * sizeof(float));

  st->Ed = 0.0f;
  st->Eu2 = 0.0f;
  st->Edu2 = 0.0f;

  st->last_gamma = 0;
  st->last_p = 0;
  st->last_mu = 0;
  st->last_eta = 0;
  st->last_y = 0;
}

float gsc_process_sample(GscState *st, const GscConfig *cfg, float xL, float xR,
                         float xB) {
  int M = st->M;
  int p = st->p_idx;

  // 1. Calculate inputs
  float mid = 0.5f * (xL + xR);
  float d = mid;
  float u1 = xL - xR;
  float u2 = mid - st->beta * xB;

  // 2. Update history buffer
  st->u1_hist[p] = u1;
  st->u2_hist[p] = u2;

  // 3. Filter (Convolution)
  float yhat = 0.0f;

// #ifdef __AVX2__
#if 0  // Force scalar for stability check
  // Limit stack buffer usage. If M > 256, strictly fallback or malloc (avoided
  // here for speed)
  if (M <= 256) {
    float u1_lin[256];
    float u2_lin[256];

    // Linearize history buffers for SIMD (reverse order: newest at 0)
    // u_lin[k] corresponds to u[n-k]
    // Part 1: p down to 0  -> maps to k=0...(p)
    // Part 2: M-1 down to p+1 -> maps to k=(p+1)...(M-1)

    int k_lin = 0;
    // [p...0]
    for (int i = p; i >= 0; i--) {
      u1_lin[k_lin] = st->u1_hist[i];
      u2_lin[k_lin] = st->u2_hist[i];
      k_lin++;
    }
    // [M-1...p+1]
    for (int i = M - 1; i > p; i--) {
      u1_lin[k_lin] = st->u1_hist[i];
      u2_lin[k_lin] = st->u2_hist[i];
      k_lin++;
    }

    // SIMD Convolution
    int M_simd = M - (M % 8);
    __m256 v_yhat = _mm256_setzero_ps();

    for (int k = 0; k < M_simd; k += 8) {
      __m256 v_w1 = _mm256_loadu_ps(&st->w1[k]);
      __m256 v_u1 = _mm256_loadu_ps(&u1_lin[k]);
      v_yhat = _mm256_fmadd_ps(v_w1, v_u1, v_yhat);

      __m256 v_w2 = _mm256_loadu_ps(&st->w2[k]);
      __m256 v_u2 = _mm256_loadu_ps(&u2_lin[k]);
      v_yhat = _mm256_fmadd_ps(v_w2, v_u2, v_yhat);
    }

    // Horizontal sum
    float temp[8];
    _mm256_storeu_ps(temp, v_yhat);
    for (int i = 0; i < 8; i++)
      yhat += temp[i];

    // Tails
    for (int k = M_simd; k < M; k++) {
      yhat += st->w1[k] * u1_lin[k];
      yhat += st->w2[k] * u2_lin[k];
    }

    // 4. Error output
    float e = d - yhat;

    // 5. Leakage Detection (EWMA)
    st->Ed = (1.0f - cfg->alpha) * st->Ed + cfg->alpha * d * d;
    st->Eu2 = (1.0f - cfg->alpha) * st->Eu2 + cfg->alpha * u2 * u2;
    st->Edu2 = (1.0f - cfg->alpha) * st->Edu2 + cfg->alpha * d * u2;

    float denom = fast_sqrtf(st->Ed * st->Eu2) + cfg->eps;
    float gamma = st->Edu2 / denom;
    float g = fast_absf(gamma);

    // 6. Soft Rate Control
    float p_control = 0.0f;
    if (g <= cfg->g_lo) {
      p_control = 0.0f;
    } else if (g >= cfg->g_hi) {
      p_control = 1.0f;
    } else {
      p_control = (g - cfg->g_lo) / (cfg->g_hi - cfg->g_lo);
    }

    float one_minus_p = 1.0f - p_control;
    float muAIC = cfg->mu_max * one_minus_p * one_minus_p;
    float etaBeta = cfg->eta_max * p_control * p_control;

    // 7. AIC Update (Leaky NLMS) - Optimized
    float Pu = 0.0f;
    // Calculate power over the window M using linear buffers
    __m256 v_Pu = _mm256_setzero_ps();
    for (int k = 0; k < M_simd; k += 8) {
      __m256 v_u1 = _mm256_loadu_ps(&u1_lin[k]);
      v_Pu = _mm256_fmadd_ps(v_u1, v_u1, v_Pu);
      __m256 v_u2 = _mm256_loadu_ps(&u2_lin[k]);
      v_Pu = _mm256_fmadd_ps(v_u2, v_u2, v_Pu);
    }
    _mm256_storeu_ps(temp, v_Pu);
    for (int i = 0; i < 8; i++)
      Pu += temp[i];
    for (int k = M_simd; k < M; k++) {
      Pu += u1_lin[k] * u1_lin[k];
      Pu += u2_lin[k] * u2_lin[k];
    }

    float norm = Pu + cfg->eps;
    float factor = muAIC * e / norm;
    float leak = 1.0f - cfg->leak_lambda;

    __m256 v_factor = _mm256_set1_ps(factor);
    __m256 v_leak = _mm256_set1_ps(leak);

    for (int k = 0; k < M_simd; k += 8) {
      __m256 v_w1 = _mm256_loadu_ps(&st->w1[k]);
      __m256 v_u1 = _mm256_loadu_ps(&u1_lin[k]);
      // w = leak*w + factor*u
      v_w1 = _mm256_fmadd_ps(v_leak, v_w1, _mm256_mul_ps(v_factor, v_u1));
      _mm256_storeu_ps(&st->w1[k], v_w1);

      __m256 v_w2 = _mm256_loadu_ps(&st->w2[k]);
      __m256 v_u2 = _mm256_loadu_ps(&u2_lin[k]);
      v_w2 = _mm256_fmadd_ps(v_leak, v_w2, _mm256_mul_ps(v_factor, v_u2));
      _mm256_storeu_ps(&st->w2[k], v_w2);
    }
    for (int k = M_simd; k < M; k++) {
      st->w1[k] = leak * st->w1[k] + factor * u1_lin[k];
      st->w2[k] = leak * st->w2[k] + factor * u2_lin[k];
    }

    // 8. Beta Update (1-tap NLMS)
    float factor_beta = etaBeta * (xB * u2) / (xB * xB + cfg->eps);
    st->beta += factor_beta;
    st->beta = clampf(st->beta, cfg->beta_min, cfg->beta_max);

    // 9. Advance index
    st->p_idx = (p + 1) % M;

    // Debug stats copy
    st->last_gamma = gamma;
    st->last_p = p_control;
    st->last_mu = muAIC;
    st->last_eta = etaBeta;
    st->last_y = e;

    return e;
  }
#endif // force scalar

  // Scalar Fallback (also used if M > 256 or AVX2 disabled)
  for (int k = 0; k < M; k++) {
    int idx = (p - k + M) % M;
    yhat += st->w1[k] * st->u1_hist[idx];
    yhat += st->w2[k] * st->u2_hist[idx];
  }

  // 4. Error output
  float e = d - yhat;

  // 5. Leakage Detection (EWMA)
  st->Ed = (1.0f - cfg->alpha) * st->Ed + cfg->alpha * d * d;
  st->Eu2 = (1.0f - cfg->alpha) * st->Eu2 + cfg->alpha * u2 * u2;
  st->Edu2 = (1.0f - cfg->alpha) * st->Edu2 + cfg->alpha * d * u2;

  float denom = fast_sqrtf(st->Ed * st->Eu2) + cfg->eps;
  float gamma = st->Edu2 / denom;
  float g = fast_absf(gamma);

  // 6. Soft Rate Control
  float p_control = 0.0f;
  if (g <= cfg->g_lo) {
    p_control = 0.0f;
  } else if (g >= cfg->g_hi) {
    p_control = 1.0f;
  } else {
    p_control = (g - cfg->g_lo) / (cfg->g_hi - cfg->g_lo);
  }

  float one_minus_p = 1.0f - p_control;
  float muAIC = cfg->mu_max * one_minus_p * one_minus_p;
  float etaBeta = cfg->eta_max * p_control * p_control;

  // 7. AIC Update (Leaky NLMS)
  float Pu = 0.0f;
  // Calculate power over the window M
  for (int k = 0; k < M; k++) {
    Pu += st->u1_hist[k] * st->u1_hist[k];
    Pu += st->u2_hist[k] * st->u2_hist[k];
  }

  float norm = Pu + cfg->eps;
  float factor = muAIC * e / norm;
  float leak = 1.0f - cfg->leak_lambda;

  for (int k = 0; k < M; k++) {
    int idx = (p - k + M) % M;
    st->w1[k] = leak * st->w1[k] + factor * st->u1_hist[idx];
    st->w2[k] = leak * st->w2[k] + factor * st->u2_hist[idx];
  }

  // 8. Beta Update (1-tap NLMS)
  float factor_beta = etaBeta * (xB * u2) / (xB * xB + cfg->eps);
  st->beta += factor_beta;
  st->beta = clampf(st->beta, cfg->beta_min, cfg->beta_max);

  // 9. Advance index
  st->p_idx = (p + 1) % M;

  // Debug stats copy
  st->last_gamma = gamma;
  st->last_p = p_control;
  st->last_mu = muAIC;
  st->last_eta = etaBeta;
  st->last_y = e;

  return e;
}
