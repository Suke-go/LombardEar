/**
 * DSP Benchmark Tool
 *
 * Measures processing time for each DSP module.
 * Usage: benchmark_dsp.exe
 */

#include "../src/dsp/biquad.h"
#include "../src/dsp/doa.h"
#include "../src/dsp/fast_math.h"
#include "../src/dsp/multiband.h"
#include "../src/dsp/steer_fast.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
static double get_time_us(void) {
  LARGE_INTEGER freq, counter;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&counter);
  return (double)counter.QuadPart * 1000000.0 / (double)freq.QuadPart;
}
#else
#include <sys/time.h>
static double get_time_us(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000.0 + tv.tv_usec;
}
#endif

#define WARMUP_ITERS 1000
#define BENCH_ITERS 100000
#define SAMPLE_RATE 48000

// Test data
static float test_samples[BATCH_SIZE * 4];

static void fill_test_data(void) {
  for (int i = 0; i < BATCH_SIZE * 4; i++) {
    test_samples[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
  }
}

// Benchmark: Fast Sin/Cos LUT
static void bench_fast_sincos(void) {
  float sum = 0;
  double t0, t1;

  // Warmup
  for (int i = 0; i < WARMUP_ITERS; i++) {
    sum += fast_sinf(i * 0.01f);
  }

  t0 = get_time_us();
  for (int i = 0; i < BENCH_ITERS; i++) {
    sum += fast_sinf(i * 0.01f);
    sum += fast_cosf(i * 0.01f);
  }
  t1 = get_time_us();

  double time_per_call = (t1 - t0) / (BENCH_ITERS * 2);
  printf("fast_sinf/cosf:   %.3f us/call (%.0f calls/sec)\n", time_per_call,
         1000000.0 / time_per_call);
  (void)sum;
}

// Benchmark: Beam Steering (LUT)
static void bench_steer_fast(void) {
  float sum = 0;
  double t0, t1;

  // Warmup
  for (int i = 0; i < WARMUP_ITERS; i++) {
    sum += steer_beam_fast(0.1f, 0.2f, -0.1f, -0.2f, i % 360);
  }

  t0 = get_time_us();
  for (int i = 0; i < BENCH_ITERS; i++) {
    float xTL = test_samples[i % BATCH_SIZE];
    float xTR = test_samples[(i + 64) % BATCH_SIZE];
    float xBL = test_samples[(i + 128) % BATCH_SIZE];
    float xBR = test_samples[(i + 192) % BATCH_SIZE];
    sum += steer_beam_fast(xTL, xTR, xBL, xBR, i % 360);
  }
  t1 = get_time_us();

  double time_per_sample = (t1 - t0) / BENCH_ITERS;
  double samples_per_sec = 1000000.0 / time_per_sample;
  printf("steer_beam_fast:  %.3f us/sample (%.0f samples/sec = %.1fx realtime "
         "@%dHz)\n",
         time_per_sample, samples_per_sec, samples_per_sec / SAMPLE_RATE,
         SAMPLE_RATE);
  (void)sum;
}

// Benchmark: Batch Steering
static void bench_steer_batch(void) {
  Mic4Batch in;
  OutputBatch out;
  double t0, t1;

  // Fill input
  for (int i = 0; i < BATCH_SIZE; i++) {
    in.xTL[i] = test_samples[i];
    in.xTR[i] = test_samples[i + 64];
    in.xBL[i] = test_samples[i + 128];
    in.xBR[i] = test_samples[i + 192];
  }

  // Warmup
  for (int i = 0; i < WARMUP_ITERS; i++) {
    steer_batch_process(&in, &out, 45, BATCH_SIZE);
  }

  int batch_iters = BENCH_ITERS / BATCH_SIZE;
  t0 = get_time_us();
  for (int i = 0; i < batch_iters; i++) {
    steer_batch_process(&in, &out, i % 360, BATCH_SIZE);
  }
  t1 = get_time_us();

  double time_per_batch = (t1 - t0) / batch_iters;
  double time_per_sample = time_per_batch / BATCH_SIZE;
  double samples_per_sec = 1000000.0 / time_per_sample;
  printf("steer_batch (%d): %.3f us/batch, %.4f us/sample (%.0f samples/sec = "
         "%.1fx realtime)\n",
         BATCH_SIZE, time_per_batch, time_per_sample, samples_per_sec,
         samples_per_sec / SAMPLE_RATE);
}

// Benchmark: Biquad Filter
static void bench_biquad(void) {
  BiquadState bq;
  biquad_lowpass(&bq, 48000.0f, 1000.0f);

  float sum = 0;
  double t0, t1;

  // Warmup
  for (int i = 0; i < WARMUP_ITERS; i++) {
    sum += biquad_process(&bq, test_samples[i % BATCH_SIZE]);
  }

  t0 = get_time_us();
  for (int i = 0; i < BENCH_ITERS; i++) {
    sum += biquad_process(&bq, test_samples[i % BATCH_SIZE]);
  }
  t1 = get_time_us();

  double time_per_sample = (t1 - t0) / BENCH_ITERS;
  printf("biquad_process:   %.3f us/sample (%.0f samples/sec)\n",
         time_per_sample, 1000000.0 / time_per_sample);
  (void)sum;
}

// Benchmark: Multiband EQ
static void bench_multiband(void) {
  MultibandState mb;
  multiband_init(&mb, 48000.0f);

  float sum = 0;
  double t0, t1;

  // Warmup
  for (int i = 0; i < WARMUP_ITERS; i++) {
    sum += multiband_process(&mb, test_samples[i % BATCH_SIZE]);
  }

  t0 = get_time_us();
  for (int i = 0; i < BENCH_ITERS; i++) {
    sum += multiband_process(&mb, test_samples[i % BATCH_SIZE]);
  }
  t1 = get_time_us();

  double time_per_sample = (t1 - t0) / BENCH_ITERS;
  printf("multiband (4band): %.3f us/sample (%.0f samples/sec)\n",
         time_per_sample, 1000000.0 / time_per_sample);
  (void)sum;
}

// Benchmark: DOA Estimation
static void bench_doa(void) {
  DoaState doa;
  doa_init(&doa, 0.02f, 5.0f); // alpha=0.02, slew_rate=5 deg/sample

  float theta;
  double t0, t1;

  // Warmup
  for (int i = 0; i < WARMUP_ITERS; i++) {
    theta = doa_update(&doa, 0.1f, 0.2f, -0.1f, -0.2f);
  }

  t0 = get_time_us();
  for (int i = 0; i < BENCH_ITERS; i++) {
    float xTL = test_samples[i % BATCH_SIZE];
    float xTR = test_samples[(i + 64) % BATCH_SIZE];
    float xBL = test_samples[(i + 128) % BATCH_SIZE];
    float xBR = test_samples[(i + 192) % BATCH_SIZE];
    theta = doa_update(&doa, xTL, xTR, xBL, xBR);
  }
  t1 = get_time_us();

  double time_per_sample = (t1 - t0) / BENCH_ITERS;
  printf("doa_update:       %.3f us/sample (%.0f samples/sec)\n",
         time_per_sample, 1000000.0 / time_per_sample);
  (void)theta;
}

// Full Pipeline Benchmark
static void bench_full_pipeline(void) {
  DoaState doa;
  MultibandState mb;
  doa_init(&doa, 0.02f, 5.0f);
  multiband_init(&mb, 48000.0f);

  float sum = 0;
  double t0, t1;

  t0 = get_time_us();
  for (int i = 0; i < BENCH_ITERS; i++) {
    float xTL = test_samples[i % BATCH_SIZE];
    float xTR = test_samples[(i + 64) % BATCH_SIZE];
    float xBL = test_samples[(i + 128) % BATCH_SIZE];
    float xBR = test_samples[(i + 192) % BATCH_SIZE];

    // DOA estimation
    float theta = doa_update(&doa, xTL, xTR, xBL, xBR);
    int theta_idx = steer_deg_to_idx(theta);

    // Beam steering
    float steered = steer_beam_fast(xTL, xTR, xBL, xBR, theta_idx);

    // Multiband EQ
    sum += multiband_process(&mb, steered);
  }
  t1 = get_time_us();

  double time_per_sample = (t1 - t0) / BENCH_ITERS;
  double samples_per_sec = 1000000.0 / time_per_sample;
  printf("\n=== FULL PIPELINE (DOA + Steer + Multiband) ===\n");
  printf("Time per sample:  %.3f us\n", time_per_sample);
  printf("Throughput:       %.0f samples/sec\n", samples_per_sec);
  printf("Realtime factor:  %.1fx @ 48kHz\n", samples_per_sec / 48000.0);
  printf("Latency budget:   %.1f%% used @ 64-sample buffer\n",
         (time_per_sample * 64) / (64.0 / 48000.0 * 1000000.0) * 100.0);
  (void)sum;
}

int main(void) {
  printf("=== LombardEar DSP Benchmark ===\n");
  printf("Iterations: %d, Sample rate: %d Hz\n\n", BENCH_ITERS, SAMPLE_RATE);

  // Initialize
  srand(12345);
  fill_test_data();
  fast_math_init();
  steer_lut_init();

  // Run benchmarks
  bench_fast_sincos();
  bench_steer_fast();
  bench_steer_batch();
  bench_biquad();
  bench_multiband();
  bench_doa();
  bench_full_pipeline();

  return 0;
}
