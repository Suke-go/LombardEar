#include "../src/dsp/gsc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Build with: gcc -I./src tests/test_gsc_offline.c src/dsp/gsc.c -o test_gsc
// -lm

#define PI 3.1415926535f

const char *dir_names[] = {"FRONT", "BACK", "LEFT", "RIGHT"};

int main() {
  // Config
  GscConfig cfg = {.M = 64,
                   .alpha = 0.005f,
                   .eps = 1e-6f,
                   .mu_max = 0.05f,
                   .eta_max = 0.001f,
                   .leak_lambda = 0.0001f,
                   .g_lo = 0.1f,
                   .g_hi = 0.3f,
                   .beta_min = -2.0f,
                   .beta_max = 2.0f};

  // Allocate state
  size_t mem_size = 4 * cfg.M * sizeof(float);
  void *mem = malloc(mem_size);
  GscState st;
  if (gsc_init(&st, &cfg, mem, mem_size) != 0) {
    fprintf(stderr, "Failed to init GSC\n");
    free(mem);
    return 1;
  }

  // Simulation parameters
  int sample_rate = 16000;
  int duration_sec = 2;
  int num_samples = sample_rate * duration_sec;

  // Test each direction
  for (int dir_idx = 0; dir_idx < 4; dir_idx++) {
    BeamDirection dir = (BeamDirection)dir_idx;
    gsc_reset(&st);

    printf("\n=== Testing Direction: %s ===\n", dir_names[dir_idx]);
    printf("Time,Target,Output,SNR_dB,Beta,Gamma\n");

    float sum_target_sq = 0, sum_error_sq = 0;

    for (int i = 0; i < num_samples; i++) {
      float t = (float)i / sample_rate;

      // Target speech: 300Hz + 600Hz harmonic with AM envelope
      float am = 1.0f + 0.5f * sinf(2.0f * PI * 2.0f * t);
      float target = am * (0.6f * sinf(2.0f * PI * 300.0f * t) +
                           0.4f * sinf(2.0f * PI * 600.0f * t));

      // Interference: 1000Hz tone + noise
      float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
      float interf = 0.5f * sinf(2.0f * PI * 1000.0f * t) + 0.3f * noise;

      // Simulate microphone inputs based on direction being tested
      // Target comes from the selected direction, interference from opposite
      float xTL, xTR, xBL, xBR;

      switch (dir) {
      case BEAM_DIR_FRONT:
        // Target from front, interference from back
        xTL = target + 0.2f * interf;
        xTR = target + 0.2f * interf;
        xBL = 0.2f * target + interf;
        xBR = 0.2f * target + interf;
        break;
      case BEAM_DIR_BACK:
        // Target from back, interference from front
        xTL = 0.2f * target + interf;
        xTR = 0.2f * target + interf;
        xBL = target + 0.2f * interf;
        xBR = target + 0.2f * interf;
        break;
      case BEAM_DIR_LEFT:
        // Target from left, interference from right
        xTL = target + 0.2f * interf;
        xTR = 0.2f * target + interf;
        xBL = target + 0.2f * interf;
        xBR = 0.2f * target + interf;
        break;
      case BEAM_DIR_RIGHT:
      default:
        // Target from right, interference from left
        xTL = 0.2f * target + interf;
        xTR = target + 0.2f * interf;
        xBL = 0.2f * target + interf;
        xBR = target + 0.2f * interf;
        break;
      }

      float y = gsc_process_sample_4ch(&st, &cfg, xTL, xTR, xBL, xBR, dir);

      // Accumulate for SNR calculation
      sum_target_sq += target * target;
      sum_error_sq += (y - target) * (y - target);

      // Print samples periodically
      if (i % 1000 == 0 && i > 0) {
        float snr = 10.0f * log10f(sum_target_sq / (sum_error_sq + 1e-10f));
        printf("%.3f,%.4f,%.4f,%.2f,%.4f,%.4f\n", t, target, y, snr, st.beta,
               st.last_gamma);
      }
    }

    // Final SNR
    float final_snr = 10.0f * log10f(sum_target_sq / (sum_error_sq + 1e-10f));
    printf("Final SNR for %s: %.2f dB\n", dir_names[dir_idx], final_snr);
  }

  free(mem);
  printf("\n=== 4-Channel GSC Test Complete ===\n");
  return 0;
}
