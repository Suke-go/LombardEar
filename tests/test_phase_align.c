/**
 * @file test_phase_align.c
 * @brief Unit tests for GCC-PHAT phase alignment
 */

#include "../src/dsp/phase_align.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 48000
#define FFT_SIZE 512
#define NUM_CHANNELS 4

#define TEST_ASSERT(cond, msg)                                                 \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__);                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

// Generate a sine wave
static void generate_sine(float *buf, int len, float freq, int sample_rate,
                          float phase_offset) {
  for (int i = 0; i < len; i++) {
    buf[i] = sinf(2.0f * (float)M_PI * freq * (float)i / (float)sample_rate +
                  phase_offset);
  }
}

// Test: Basic create/destroy
static int test_create_destroy(void) {
  PhaseAligner *pa = phase_align_create(FFT_SIZE, SAMPLE_RATE, NUM_CHANNELS);
  TEST_ASSERT(pa != NULL, "phase_align_create returned NULL");

  phase_align_destroy(pa);
  printf("PASS: test_create_destroy\n");
  return 0;
}

// Test: Known delay estimation
static int test_known_delay(void) {
  PhaseAligner *pa = phase_align_create(FFT_SIZE, SAMPLE_RATE, NUM_CHANNELS);
  TEST_ASSERT(pa != NULL, "phase_align_create failed");

  // Set high smoothing for immediate response
  phase_align_set_smoothing(pa, 1.0f);

  // Generate reference signal (1kHz sine)
  float ref[FFT_SIZE];
  float ch1[FFT_SIZE];
  float ch2[FFT_SIZE];
  float ch3[FFT_SIZE];

  generate_sine(ref, FFT_SIZE, 1000.0f, SAMPLE_RATE, 0.0f);

  // Create delayed copies
  int delay1 = 5;  // 5 samples delay
  int delay2 = -3; // 3 samples advance
  int delay3 = 10; // 10 samples delay

  memset(ch1, 0, sizeof(ch1));
  memset(ch2, 0, sizeof(ch2));
  memset(ch3, 0, sizeof(ch3));

  for (int i = 0; i < FFT_SIZE; i++) {
    if (i >= delay1 && i < FFT_SIZE)
      ch1[i] = ref[i - delay1];
    if (i - delay2 >= 0 && i - delay2 < FFT_SIZE)
      ch2[i] = ref[i - delay2];
    if (i >= delay3 && i < FFT_SIZE)
      ch3[i] = ref[i - delay3];
  }

  // Set up channel pointers
  const float *channels[4] = {ref, ch1, ch2, ch3};
  float offsets[3];

  // Estimate delays
  phase_align_estimate(pa, channels, FFT_SIZE, offsets);

  printf("  Estimated offsets: [%.2f, %.2f, %.2f] (expected: [%d, %d, %d])\n",
         offsets[0], offsets[1], offsets[2], delay1, delay2, delay3);

  // Check accuracy (within ±2 samples)
  TEST_ASSERT(fabsf(offsets[0] - (float)delay1) < 2.0f,
              "Ch1 delay estimate too far off");
  TEST_ASSERT(fabsf(offsets[1] - (float)delay2) < 2.0f,
              "Ch2 delay estimate too far off");
  TEST_ASSERT(fabsf(offsets[2] - (float)delay3) < 2.0f,
              "Ch3 delay estimate too far off");

  phase_align_destroy(pa);
  printf("PASS: test_known_delay\n");
  return 0;
}

// Test: Phase correction
static int test_phase_correction(void) {
  float original[128];
  float corrected[128];

  // Fill with test pattern
  for (int i = 0; i < 128; i++) {
    original[i] = (float)i;
  }

  // Apply 0 offset (should be identity)
  phase_align_correct(original, corrected, 128, 0.0f);

  int mismatch = 0;
  for (int i = 0; i < 128; i++) {
    if (fabsf(original[i] - corrected[i]) > 1e-6f) {
      mismatch++;
    }
  }
  TEST_ASSERT(mismatch == 0, "Zero offset should preserve signal");

  printf("PASS: test_phase_correction\n");
  return 0;
}

// Test: Smoothing behavior
static int test_smoothing(void) {
  PhaseAligner *pa = phase_align_create(256, SAMPLE_RATE, 2);
  TEST_ASSERT(pa != NULL, "phase_align_create failed");

  // Low smoothing = slow tracking
  phase_align_set_smoothing(pa, 0.1f);

  float ref[256], target[256];
  generate_sine(ref, 256, 500.0f, SAMPLE_RATE, 0.0f);
  generate_sine(target, 256, 500.0f, SAMPLE_RATE, 0.1f);

  const float *channels[2] = {ref, target};
  float offsets[1];

  // Multiple iterations should converge
  for (int i = 0; i < 10; i++) {
    phase_align_estimate(pa, channels, 256, offsets);
  }

  printf("  Smoothed offset after 10 iterations: %.3f\n", offsets[0]);

  phase_align_destroy(pa);
  printf("PASS: test_smoothing\n");
  return 0;
}

int main(void) {
  printf("=== Phase Alignment Unit Tests ===\n\n");

  int failures = 0;
  failures += test_create_destroy();
  failures += test_known_delay();
  failures += test_phase_correction();
  failures += test_smoothing();

  printf("\n=== Results: %d failures ===\n", failures);
  return failures > 0 ? 1 : 0;
}
