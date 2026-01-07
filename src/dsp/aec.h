#ifndef AEC_H
#define AEC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float *w;                   // Filter coefficients (length M)
  float *x_history;           // Reference signal history (length M)
  int M;                      // Filter length
  int write_idx;              // Circular buffer index
  float mu;                   // Step size
  float param_regularization; // Regularization parameter for NLMS (eps)
  float power_est;            // Power estimate of reference signal
} AecState;

/**
 * Initialize AEC state.
 * @param st: State structure
 * @param filter_len: Length of adaptive filter (e.g., 256 or 512)
 * @param mem: Memory buffer provided by caller
 * @param mem_size: Size of memory buffer in bytes
 * @return 0 on success, -1 if memory insufficient
 */
int aec_init(AecState *st, int filter_len, float *mem, size_t mem_size);

/**
 * Process one sample.
 * @param st: State structure
 * @param mic_in: Signal from microphone (Input)
 * @param ref_in: Signal sent to speaker (Reference)
 * @return Echo-cancelled signal (Error signal)
 */
float aec_process(AecState *st, float mic_in, float ref_in);

/**
 * Set adaptation step size.
 * @param st: State structure
 * @param mu: Step size (0.0 to 1.0)
 */
void aec_set_step_size(AecState *st, float mu);

#ifdef __cplusplus
}
#endif

#endif // AEC_H
