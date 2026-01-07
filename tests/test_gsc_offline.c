#include "../src/dsp/gsc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


// Build with: gcc -I./src tests/test_gsc_offline.c src/dsp/gsc.c -o test_gsc
// -lm

#define PI 3.1415926535f

int main() {
  // Config
  GscConfig cfg = {.M = 64,
                   .alpha = 0.005f, // Fast tracking for test
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
  int duration_sec = 3;
  int num_samples = sample_rate * duration_sec;

  printf("Time,Target,Output,Beta,Gamma,P_Control\n");

  for (int i = 0; i < num_samples; i++) {
    float t = (float)i / sample_rate;

    // Scenario:
    // Target s: Voice (Fundamental 300Hz + Harmonic) + AM modulation
    // (Speech-like envelope)
    float am = 1.0f + 0.5f * sinf(2.0f * PI * 2.0f * t); // 2Hz AM envelope
    float s = am * (0.6f * sinf(2.0f * PI * 300.0f * t) +
                    0.4f * sinf(2.0f * PI * 600.0f * t));

    // Interference v: Noise + 1000Hz tone
    float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    float v = 0.5f * sinf(2.0f * PI * 1000.0f * t) + 0.3f * noise;

    // Mic inputs
    // d (FBF) = s
    // xL = s + 0.3v
    // xR = s - 0.3v  -> FBF d = 0.5*(2s) = s. u1 = 0.6v.
    // xB = 0.8v + 0.2s (Stronger leakage: 20% target s leaks into reference xB)
    float xL = s + 0.3f * v;
    float xR = s - 0.3f * v;
    float xB = 0.8f * v + 0.2f * s; // Intentional leakage

    float y = gsc_process_sample(&st, &cfg, xL, xR, xB);

    // Print CSV samples mainly when leakage is active (always in this case)
    if (i % 100 == 0) {
      printf("%.3f,%.4f,%.4f,%.4f,%.4f,%.4f\n", t, s, y, st.beta, st.last_gamma,
             st.last_p);
    }
  }

  // We expect P_Control to be high (close to 1) because of leakage (Gamma high)
  // Beta should adapt to cancel v from u2 (u2 = d - beta*xB? No, u2 = d -
  // beta*xB is wrong. Spec: u2 = mid - beta * xB. mid = s. xB = 0.8v + 0.2s. If
  // beta -> 0, u2 = s. Leaks massively. Ideally beta should remove correlated
  // component? Wait, u2 is "Reference 2" (Blocking Matrix output). Usually BM
  // aims to BLOCK target. u2 = mid - beta*xB. If mid=s, xB has s. We want u2 to
  // have NO s. So s - beta * (0.8v + 0.2s) = s * (1 - 0.2beta) - 0.8*beta*v. To
  // block s, 1 - 0.2beta = 0 => beta = 5. But beta_max is 2. So it saturates.
  // If beta=2, s * (1 - 0.4) = 0.6s. Still leaks.
  // GSC detects this Leakage (Gamma between d=s and u2).
  // Gamma should be high.
  // P should be high.
  // AIC update should be suppressed (muAIC -> 0).
  // So y should remain close to d (which is s).
  // i.e., Target Preservation.

  free(mem);
  return 0;
}
