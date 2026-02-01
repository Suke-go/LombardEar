#ifndef RING_BUFFER_H
#define RING_BUFFER_H

/**
 * Lock-Free Ring Buffer for Real-Time Audio
 *
 * Features:
 * - Single producer, single consumer (SPSC)
 * - Cache-line aligned to avoid false sharing
 * - Power-of-2 sizing for fast modulo
 */

#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Cache Line Alignment
// ============================================================================

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#ifdef _MSC_VER
#define CACHE_ALIGNED __declspec(align(64))
#else
#define CACHE_ALIGNED __attribute__((aligned(64)))
#endif

// ============================================================================
// Ring Buffer (Float samples)
// ============================================================================

typedef struct {
  float *buffer;
  size_t capacity; // Must be power of 2
  size_t mask;     // capacity - 1

  // Separate cache lines for producer and consumer
  CACHE_ALIGNED size_t write_pos;
  CACHE_ALIGNED size_t read_pos;
} RingBuffer;

// Initialize ring buffer with given capacity (must be power of 2)
static inline int ring_buffer_init(RingBuffer *rb, float *buffer,
                                   size_t capacity) {
  // Check power of 2
  if ((capacity & (capacity - 1)) != 0)
    return -1;

  rb->buffer = buffer;
  rb->capacity = capacity;
  rb->mask = capacity - 1;
  rb->write_pos = 0;
  rb->read_pos = 0;
  return 0;
}

// Available space for writing
static inline size_t ring_buffer_write_available(const RingBuffer *rb) {
  return rb->capacity - (rb->write_pos - rb->read_pos);
}

// Available samples for reading
static inline size_t ring_buffer_read_available(const RingBuffer *rb) {
  return rb->write_pos - rb->read_pos;
}

// Write single sample
static inline int ring_buffer_write(RingBuffer *rb, float sample) {
  if (ring_buffer_write_available(rb) == 0)
    return -1;
  rb->buffer[rb->write_pos & rb->mask] = sample;
  rb->write_pos++;
  return 0;
}

// Read single sample
static inline int ring_buffer_read(RingBuffer *rb, float *sample) {
  if (ring_buffer_read_available(rb) == 0)
    return -1;
  *sample = rb->buffer[rb->read_pos & rb->mask];
  rb->read_pos++;
  return 0;
}

// Write batch (returns number written)
static inline size_t ring_buffer_write_batch(RingBuffer *rb, const float *data,
                                             size_t count) {
  size_t available = ring_buffer_write_available(rb);
  if (count > available)
    count = available;

  for (size_t i = 0; i < count; i++) {
    rb->buffer[(rb->write_pos + i) & rb->mask] = data[i];
  }
  rb->write_pos += count;
  return count;
}

// Read batch (returns number read)
static inline size_t ring_buffer_read_batch(RingBuffer *rb, float *data,
                                            size_t count) {
  size_t available = ring_buffer_read_available(rb);
  if (count > available)
    count = available;

  for (size_t i = 0; i < count; i++) {
    data[i] = rb->buffer[(rb->read_pos + i) & rb->mask];
  }
  rb->read_pos += count;
  return count;
}

// ============================================================================
// 4-Channel Ring Buffer (for beamformer input)
// ============================================================================

typedef struct {
  RingBuffer ch[4]; // TL, TR, BL, BR
} RingBuffer4Ch;

static inline int ring_buffer_4ch_init(RingBuffer4Ch *rb, float *buffers[4],
                                       size_t capacity) {
  for (int i = 0; i < 4; i++) {
    if (ring_buffer_init(&rb->ch[i], buffers[i], capacity) != 0)
      return -1;
  }
  return 0;
}

static inline size_t ring_buffer_4ch_read_available(const RingBuffer4Ch *rb) {
  size_t min = ring_buffer_read_available(&rb->ch[0]);
  for (int i = 1; i < 4; i++) {
    size_t avail = ring_buffer_read_available(&rb->ch[i]);
    if (avail < min)
      min = avail;
  }
  return min;
}

#ifdef __cplusplus
}
#endif

#endif // RING_BUFFER_H
