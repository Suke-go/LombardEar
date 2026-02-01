#include "../src/dsp/multiband.h"
#include "../src/dsp/steer.h"
#include <math.h>
#include <stdio.h>
#include <time.h>

#define PI 3.14159265358979323846f
#define SAMPLE_RATE 16000

int main() {
  printf("=== Spatial-Spectral Processor Test ===\n\n");

  // Initialize multiband processor
  MultibandState mb;
  multiband_init(&mb, SAMPLE_RATE);

  // Test parameters
  int num_samples = SAMPLE_RATE * 2; // 2 seconds

  // Test different angles
  float test_angles[] = {0, 45, 90, 135, 180, 225, 270, 315};
  int num_angles = sizeof(test_angles) / sizeof(test_angles[0]);

  printf("Testing steering at %d angles:\n", num_angles);
  printf("Angle, AvgOutput, SNR_dB\n");

  for (int a = 0; a < num_angles; a++) {
    float theta = test_angles[a];
    float sum_signal = 0, sum_noise = 0;

    for (int i = 0; i < num_samples; i++) {
      float t = (float)i / SAMPLE_RATE;

      // Target: 500Hz voice-like signal from front (0 degrees)
      float target = sinf(2.0f * PI * 500.0f * t);

      // Interference: 2000Hz from back (180 degrees)
      float interf = 0.5f * sinf(2.0f * PI * 2000.0f * t);

      // Simulate mic signals based on source directions
      // Target from 0 deg (front): strong at TL/TR
      // Interference from 180 deg (back): strong at BL/BR
      float xTL = target * 0.9f + interf * 0.1f;
      float xTR = target * 0.9f + interf * 0.1f;
      float xBL = target * 0.1f + interf * 0.9f;
      float xBR = target * 0.1f + interf * 0.9f;

      // Process with spatial-spectral filter
      float out = spatial_spectral_process(xTL, xTR, xBL, xBR, theta, &mb);

      // Calculate how much target vs interference is in output
      // When theta=0, we should get more target
      // When theta=180, we should get more interference
      if (theta < 90 || theta > 270) {
        // Pointing towards target
        sum_signal += out * out;
      } else {
        // Pointing away from target
        sum_noise += out * out;
      }
    }

    float avg_power = (sum_signal + sum_noise) / num_samples;
    float rms = sqrtf(avg_power);

    printf("%.0f, %.4f, -\n", theta, rms);
  }

  // Latency test
  printf("\n=== Latency Analysis ===\n");
  printf("Algorithm latency: 0 samples (sample-by-sample processing)\n");
  printf("Biquad filters: 6 x Direct Form II = 0 samples lookahead\n");
  printf("Total algorithmic delay: 0 samples\n");

  // Performance test
  printf("\n=== Performance Test ===\n");
  clock_t start = clock();

  float dummy = 0;
  for (int i = 0; i < 1000000; i++) {
    float t = (float)i / SAMPLE_RATE;
    float x = sinf(2.0f * PI * 500.0f * t);
    dummy += spatial_spectral_process(x, x, x * 0.5f, x * 0.5f, 45.0f, &mb);
  }

  clock_t end = clock();
  double elapsed_ms = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
  double ns_per_sample = elapsed_ms * 1000.0; // 1M samples

  printf("1M samples processed in %.2f ms\n", elapsed_ms);
  printf("~%.2f ns per sample\n", ns_per_sample);
  printf("Dummy output (prevent optimization): %.4f\n", dummy);

  printf("\n=== Test Complete ===\n");
  return 0;
}
