/**
 * @file test_jitter_buffer.c
 * @brief Unit tests for adaptive jitter buffer
 */

#include "../src/audio/jitter_buffer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_RATE 48000
#define CHANNELS 4
#define TARGET_DELAY_MS 100

#define TEST_ASSERT(cond, msg)                                                 \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__);                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

// Test: Basic create/destroy
static int test_create_destroy(void) {
  JitterBuffer *jb =
      jitter_buffer_create(SAMPLE_RATE, CHANNELS, TARGET_DELAY_MS);
  TEST_ASSERT(jb != NULL, "jitter_buffer_create returned NULL");

  JitterStats stats;
  jitter_buffer_get_stats(jb, &stats);
  TEST_ASSERT(stats.underruns == 0, "Initial underruns should be 0");
  TEST_ASSERT(stats.fill_ratio < 0.01f, "Initial fill ratio should be ~0");

  jitter_buffer_destroy(jb);
  printf("PASS: test_create_destroy\n");
  return 0;
}

// Test: Write and read samples
static int test_write_read(void) {
  JitterBuffer *jb =
      jitter_buffer_create(SAMPLE_RATE, CHANNELS, TARGET_DELAY_MS);
  TEST_ASSERT(jb != NULL, "jitter_buffer_create failed");

  // Generate test data
  int num_frames = 256;
  float *write_buf = (float *)malloc(num_frames * CHANNELS * sizeof(float));
  float *read_buf = (float *)calloc(num_frames * CHANNELS, sizeof(float));

  for (int i = 0; i < num_frames * CHANNELS; i++) {
    write_buf[i] = (float)(i % 100) / 100.0f;
  }

  // Write samples
  int written = jitter_buffer_write(jb, write_buf, num_frames, 0);
  TEST_ASSERT(written == num_frames, "Should write all samples");

  JitterStats stats;
  jitter_buffer_get_stats(jb, &stats);
  TEST_ASSERT(stats.fill_ratio > 0, "Fill ratio should increase after write");

  // Read samples
  int read = jitter_buffer_read(jb, read_buf, num_frames);
  TEST_ASSERT(read == num_frames, "Should read all samples");

  // Verify data integrity
  int mismatch = 0;
  for (int i = 0; i < num_frames * CHANNELS; i++) {
    if (fabsf(write_buf[i] - read_buf[i]) > 1e-6f) {
      mismatch++;
    }
  }
  TEST_ASSERT(mismatch == 0, "Read data should match written data");

  free(write_buf);
  free(read_buf);
  jitter_buffer_destroy(jb);
  printf("PASS: test_write_read\n");
  return 0;
}

// Test: Underrun handling
static int test_underrun(void) {
  JitterBuffer *jb =
      jitter_buffer_create(SAMPLE_RATE, CHANNELS, TARGET_DELAY_MS);
  TEST_ASSERT(jb != NULL, "jitter_buffer_create failed");

  // Try to read without writing (should cause underrun)
  float read_buf[256 * CHANNELS];
  int read = jitter_buffer_read(jb, read_buf, 256);
  TEST_ASSERT(read == 256, "Should still return requested samples");

  JitterStats stats;
  jitter_buffer_get_stats(jb, &stats);
  TEST_ASSERT(stats.underruns > 0, "Should count underrun");

  jitter_buffer_destroy(jb);
  printf("PASS: test_underrun\n");
  return 0;
}

// Test: Statistics tracking
static int test_statistics(void) {
  JitterBuffer *jb = jitter_buffer_create(SAMPLE_RATE, CHANNELS, 50);
  TEST_ASSERT(jb != NULL, "jitter_buffer_create failed");

  float buf[64 * CHANNELS];
  memset(buf, 0, sizeof(buf));

  // Simulate packets with varying timestamps
  uint64_t ts = 0;
  for (int i = 0; i < 10; i++) {
    // Add some jitter to timestamps
    int jitter_us = (i % 3) * 500; // 0, 500, 1000 us
    jitter_buffer_write(jb, buf, 64, ts + jitter_us);
    ts += (64 * 1000000) / SAMPLE_RATE; // Expected interval
  }

  JitterStats stats;
  jitter_buffer_get_stats(jb, &stats);
  printf("  Jitter mean: %.2f ms, std: %.2f ms, drift: %.1f PPM\n",
         stats.jitter_mean_ms, stats.jitter_std_ms, stats.drift_ppm);

  jitter_buffer_destroy(jb);
  printf("PASS: test_statistics\n");
  return 0;
}

int main(void) {
  printf("=== Jitter Buffer Unit Tests ===\n\n");

  int failures = 0;
  failures += test_create_destroy();
  failures += test_write_read();
  failures += test_underrun();
  failures += test_statistics();

  printf("\n=== Results: %d failures ===\n", failures);
  return failures > 0 ? 1 : 0;
}
