#ifndef GSC_H
#define GSC_H

#include <stddef.h> // size_t

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int M;             // FIR length (e.g. 64)
  float alpha;       // EWMA factor (0.001 - 0.01)
  float eps;         // epsilon
  float mu_max;      // AIC max step size
  float eta_max;     // Beta max step size
  float leak_lambda; // Leaky NLMS factor
  float g_lo;        // Soft control low threshold
  float g_hi;        // Soft control high threshold
  float beta_min;    // Beta limit min
  float beta_max;    // Beta limit max
} GscConfig;

typedef struct {
  // State variables
  int M;
  int p_idx; // Ring buffer write index
  float beta;

  // Arrays (pointers to provided memory)
  float *w1;      // [M]
  float *w2;      // [M]
  float *u1_hist; // [M]
  float *u2_hist; // [M]

  // Leakage EWMA states
  float Ed;
  float Eu2;
  float Edu2;

  // Debug / Monitoring
  float last_gamma;
  float last_p;
  float last_mu;
  float last_eta;
  float last_y;

} GscState;

// Initialize GSC state.
// mem: Pointer to allocated memory block.
// mem_bytes: Size of the block. Must be at least 4 * M * sizeof(float).
// Returns 0 on success, -1 on error (insufficient memory).
int gsc_init(GscState *st, const GscConfig *cfg, void *mem, size_t mem_bytes);

void gsc_reset(GscState *st);

// Process one sample set.
// xL, xR: Left/Right mic inputs
// xB: Back mic input
// Returns: Processed output sample e[n]
float gsc_process_sample(GscState *st, const GscConfig *cfg, float xL, float xR,
                         float xB);

#ifdef __cplusplus
}
#endif

#endif // GSC_H
