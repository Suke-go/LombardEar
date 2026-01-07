#include "../src/dsp/aec.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


// Simple random generator
float randf() { return ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f; }

int main() {
  printf("Testing AEC Offline...\n");

  int M = 256; // Filter length
  size_t mem_size = 2 * M * sizeof(float);
  float *mem = malloc(mem_size);

  AecState aec;
  aec_init(&aec, M, mem, mem_size);

  // Simulate Echo Path (Simple delay + attenuation)
  // Delay 10 samples, attenuation 0.5
  int delay = 10;
  float attenuation = 0.5f;
#define BUF_SIZE 1024
  float ref_hist[BUF_SIZE] = {0};
  int rh_idx = 0;

  // Stats
  double sum_mic = 0;
  double sum_err = 0;

  int frames = 20000;
  for (int i = 0; i < frames; i++) {
    // Far-end signal (Reference)
    float x = randf() * 0.8f;

    // Store in history for echo simulation
    ref_hist[rh_idx] = x;
    rh_idx = (rh_idx + 1) % BUF_SIZE;

    // Simulate Mic Input = Near-end (Silence) + Echo
    // Echo is delayed version of x
    int echo_idx = (rh_idx - 1 - delay + BUF_SIZE) % BUF_SIZE;
    float echo = ref_hist[echo_idx] * attenuation;

    float mic = echo; // No near-end speech for this test, just echo

    // Process AEC
    // Note: AEC takes (mic, ref). Ideally reference should be aligned or AEC
    // handles it. Our AEC is NLMS. It tries to estimate 'mic' from 'x'.
    float err = aec_process(&aec, mic, x);

    // Converge stats after some time
    if (i > frames - 1000) {
      sum_mic += mic * mic;
      sum_err += err * err;
    }
  }

  double mse_mic = sum_mic / 1000.0;
  double mse_err = sum_err / 1000.0;
  double erle = 10.0 * log10(mse_mic / (mse_err + 1e-10));

  printf("MSE Mic (Echo Only): %.6f\n", mse_mic);
  printf("MSE Error (Residual): %.6f\n", mse_err);
  printf("ERLE: %.2f dB\n", erle);

  free(mem);

  if (erle > 10.0) {
    printf("PASS: AEC converged (ERLE > 10dB)\n");
    return 0;
  } else {
    printf("FAIL: AEC did not converge sufficiently\n");
    return 1;
  }
}
