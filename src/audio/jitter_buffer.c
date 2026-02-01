/**
 * @file jitter_buffer.c
 * @brief Adaptive Jitter Buffer implementation
 */

#include "jitter_buffer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Configuration constants
#define MAX_BUFFER_MS 500   // Maximum buffer size in ms
#define MIN_BUFFER_MS 20    // Minimum buffer size in ms
#define PLL_BANDWIDTH 0.01f // PLL loop bandwidth (slow tracking)
#define JITTER_ALPHA 0.02f  // EWMA coefficient for jitter estimation
#define DRIFT_ALPHA 0.001f  // EWMA coefficient for drift estimation

struct JitterBuffer {
  // Ring buffer
  float *buffer;
  int buffer_size; // Total samples (frames * channels)
  int buffer_frames;
  int write_pos;
  int read_pos;
  int stored_frames;

  // Configuration
  int sample_rate;
  int channels;
  int target_delay_frames;

  // PLL state for clock drift tracking
  double phase_acc;     // Accumulated phase error
  double freq_estimate; // Relative frequency (1.0 = no drift)

  // Jitter statistics
  float jitter_mean;
  float jitter_var;
  uint64_t last_timestamp;
  int64_t expected_interval_us;

  // Performance counters
  int underruns;
  int overruns;

  // Last sample for interpolation
  float *last_samples;
};

JitterBuffer *jitter_buffer_create(int sample_rate, int channels,
                                   int target_delay_ms) {
  if (sample_rate <= 0 || channels <= 0 || target_delay_ms < MIN_BUFFER_MS) {
    return NULL;
  }

  JitterBuffer *jb = (JitterBuffer *)calloc(1, sizeof(JitterBuffer));
  if (!jb)
    return NULL;

  jb->sample_rate = sample_rate;
  jb->channels = channels;
  jb->target_delay_frames = (target_delay_ms * sample_rate) / 1000;

  // Allocate buffer for MAX_BUFFER_MS
  jb->buffer_frames = (MAX_BUFFER_MS * sample_rate) / 1000;
  jb->buffer_size = jb->buffer_frames * channels;
  jb->buffer = (float *)calloc(jb->buffer_size, sizeof(float));
  if (!jb->buffer) {
    free(jb);
    return NULL;
  }

  // Allocate last sample buffer for interpolation
  jb->last_samples = (float *)calloc(channels, sizeof(float));
  if (!jb->last_samples) {
    free(jb->buffer);
    free(jb);
    return NULL;
  }

  // Initialize PLL
  jb->freq_estimate = 1.0;
  jb->phase_acc = 0.0;

  // Initialize jitter estimation
  jb->jitter_mean = 0.0f;
  jb->jitter_var = 0.0f;
  jb->last_timestamp = 0;
  jb->expected_interval_us = 0;

  return jb;
}

void jitter_buffer_destroy(JitterBuffer *jb) {
  if (jb) {
    free(jb->buffer);
    free(jb->last_samples);
    free(jb);
  }
}

void jitter_buffer_reset(JitterBuffer *jb) {
  if (!jb)
    return;

  jb->write_pos = 0;
  jb->read_pos = 0;
  jb->stored_frames = 0;

  jb->phase_acc = 0.0;
  jb->freq_estimate = 1.0;

  jb->jitter_mean = 0.0f;
  jb->jitter_var = 0.0f;
  jb->last_timestamp = 0;

  jb->underruns = 0;
  jb->overruns = 0;

  memset(jb->buffer, 0, jb->buffer_size * sizeof(float));
  memset(jb->last_samples, 0, jb->channels * sizeof(float));
}

int jitter_buffer_write(JitterBuffer *jb, const float *samples, int num_samples,
                        uint64_t timestamp_us) {
  if (!jb || !samples || num_samples <= 0)
    return 0;

  // Update jitter estimation if timestamp provided
  if (timestamp_us > 0 && jb->last_timestamp > 0) {
    int64_t interval = (int64_t)(timestamp_us - jb->last_timestamp);

    // Expected interval based on sample count
    if (jb->expected_interval_us == 0) {
      jb->expected_interval_us = interval;
    }

    // Jitter = deviation from expected
    float jitter = fabsf((float)(interval - jb->expected_interval_us));

    // Update EWMA
    jb->jitter_mean =
        (1.0f - JITTER_ALPHA) * jb->jitter_mean + JITTER_ALPHA * jitter;

    float delta = jitter - jb->jitter_mean;
    jb->jitter_var =
        (1.0f - JITTER_ALPHA) * jb->jitter_var + JITTER_ALPHA * delta * delta;

    // Update expected interval (for drift tracking)
    jb->expected_interval_us =
        (int64_t)((1.0 - DRIFT_ALPHA) * jb->expected_interval_us +
                  DRIFT_ALPHA * interval);

    // PLL: estimate frequency drift
    double error = (double)(interval - jb->expected_interval_us) /
                   (double)jb->expected_interval_us;
    jb->phase_acc += error;
    jb->freq_estimate += PLL_BANDWIDTH * error +
                         PLL_BANDWIDTH * PLL_BANDWIDTH * 0.25 * jb->phase_acc;

    // Clamp frequency estimate to reasonable range (±1000 PPM)
    if (jb->freq_estimate < 0.999)
      jb->freq_estimate = 0.999;
    if (jb->freq_estimate > 1.001)
      jb->freq_estimate = 1.001;
  }
  jb->last_timestamp = timestamp_us;

  // Check for overflow
  int available = jb->buffer_frames - jb->stored_frames;
  if (num_samples > available) {
    // Buffer overflow - drop oldest samples
    jb->overruns++;
    int drop = num_samples - available;
    jb->read_pos = (jb->read_pos + drop) % jb->buffer_frames;
    jb->stored_frames -= drop;
  }

  // Write samples to ring buffer
  int written = 0;
  while (written < num_samples) {
    int chunk = num_samples - written;
    int to_end = jb->buffer_frames - jb->write_pos;
    if (chunk > to_end)
      chunk = to_end;

    memcpy(&jb->buffer[jb->write_pos * jb->channels],
           &samples[written * jb->channels],
           chunk * jb->channels * sizeof(float));

    jb->write_pos = (jb->write_pos + chunk) % jb->buffer_frames;
    jb->stored_frames += chunk;
    written += chunk;
  }

  return written;
}

int jitter_buffer_read(JitterBuffer *jb, float *out, int num_samples) {
  if (!jb || !out || num_samples <= 0)
    return 0;

  int read = 0;

  while (read < num_samples) {
    if (jb->stored_frames > 0) {
      // Read from buffer
      int chunk = num_samples - read;
      int available = jb->stored_frames;
      if (chunk > available)
        chunk = available;

      int to_end = jb->buffer_frames - jb->read_pos;
      if (chunk > to_end)
        chunk = to_end;

      memcpy(&out[read * jb->channels],
             &jb->buffer[jb->read_pos * jb->channels],
             chunk * jb->channels * sizeof(float));

      // Update last samples for interpolation
      memcpy(jb->last_samples,
             &jb->buffer[(jb->read_pos + chunk - 1) * jb->channels],
             jb->channels * sizeof(float));

      jb->read_pos = (jb->read_pos + chunk) % jb->buffer_frames;
      jb->stored_frames -= chunk;
      read += chunk;
    } else {
      // Buffer underrun - use linear interpolation (repeat last sample)
      jb->underruns++;

      int remaining = num_samples - read;
      for (int i = 0; i < remaining; i++) {
        for (int c = 0; c < jb->channels; c++) {
          // Slight decay to avoid clicks
          out[(read + i) * jb->channels + c] = jb->last_samples[c] * 0.99f;
          jb->last_samples[c] *= 0.99f;
        }
      }
      read = num_samples;
    }
  }

  return read;
}

void jitter_buffer_get_stats(const JitterBuffer *jb, JitterStats *stats) {
  if (!jb || !stats)
    return;

  float frames_to_ms = 1000.0f / (float)jb->sample_rate;

  stats->delay_ms = (float)jb->stored_frames * frames_to_ms;
  stats->jitter_mean_ms = jb->jitter_mean / 1000.0f; // us -> ms
  stats->jitter_std_ms = sqrtf(jb->jitter_var) / 1000.0f;
  stats->fill_ratio = (float)jb->stored_frames / (float)jb->buffer_frames;
  stats->underruns = jb->underruns;
  stats->drift_ppm = (float)((jb->freq_estimate - 1.0) * 1000000.0);
}

void jitter_buffer_set_target_delay(JitterBuffer *jb, int target_delay_ms) {
  if (!jb || target_delay_ms < MIN_BUFFER_MS)
    return;

  int max_frames = (MAX_BUFFER_MS * jb->sample_rate) / 1000;
  int target_frames = (target_delay_ms * jb->sample_rate) / 1000;

  if (target_frames > max_frames)
    target_frames = max_frames;

  jb->target_delay_frames = target_frames;
}
