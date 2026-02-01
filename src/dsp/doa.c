#include "doa.h"

static inline float clampf(float v, float min, float max) {
  if (v < min)
    return min;
  if (v > max)
    return max;
  return v;
}

static inline float absf(float v) { return v < 0 ? -v : v; }

void doa_init(DoaState *st, float alpha, float slew_rate) {
  st->e_front = 0.0f;
  st->e_back = 0.0f;
  st->e_left = 0.0f;
  st->e_right = 0.0f;
  st->alpha = alpha;
  st->theta_deg = 0.0f;
  st->target_theta = 0.0f;
  st->confidence = 0.0f;
  st->slew_rate = slew_rate;
}

void doa_reset(DoaState *st) {
  st->e_front = 0.0f;
  st->e_back = 0.0f;
  st->e_left = 0.0f;
  st->e_right = 0.0f;
  st->theta_deg = 0.0f;
  st->target_theta = 0.0f;
  st->confidence = 0.0f;
}

float doa_update(DoaState *st, float xTL, float xTR, float xBL, float xBR) {
  float a = st->alpha;

  // Compute instantaneous energy for each direction pair
  float front = 0.5f * (xTL * xTL + xTR * xTR);
  float back = 0.5f * (xBL * xBL + xBR * xBR);
  float left = 0.5f * (xTL * xTL + xBL * xBL);
  float right = 0.5f * (xTR * xTR + xBR * xBR);

  // Exponential smoothing
  st->e_front = (1.0f - a) * st->e_front + a * front;
  st->e_back = (1.0f - a) * st->e_back + a * back;
  st->e_left = (1.0f - a) * st->e_left + a * left;
  st->e_right = (1.0f - a) * st->e_right + a * right;

  // Compute directional ratios
  float fb_sum = st->e_front + st->e_back + 1e-10f;
  float lr_sum = st->e_left + st->e_right + 1e-10f;

  // -1 to +1 ratios
  float fb_ratio = (st->e_front - st->e_back) / fb_sum; // +1=front, -1=back
  float lr_ratio = (st->e_right - st->e_left) / lr_sum; // +1=right, -1=left

  // Convert to angle using atan2
  // fb_ratio maps to cos(theta), lr_ratio maps to sin(theta)
  float target = atan2f(lr_ratio, fb_ratio) * (180.0f / M_PI);

  // Normalize to 0-360
  if (target < 0)
    target += 360.0f;

  st->target_theta = target;

  // Compute confidence based on energy contrast
  float max_e = st->e_front;
  if (st->e_back > max_e)
    max_e = st->e_back;
  if (st->e_left > max_e)
    max_e = st->e_left;
  if (st->e_right > max_e)
    max_e = st->e_right;

  float min_e = st->e_front;
  if (st->e_back < min_e)
    min_e = st->e_back;
  if (st->e_left < min_e)
    min_e = st->e_left;
  if (st->e_right < min_e)
    min_e = st->e_right;

  // Confidence: high when one direction dominates
  st->confidence = (max_e - min_e) / (max_e + 1e-10f);
  st->confidence = clampf(st->confidence, 0.0f, 1.0f);

  // Slew-rate limited tracking (smooth following)
  float diff = target - st->theta_deg;

  // Handle wraparound (shortest path)
  if (diff > 180.0f)
    diff -= 360.0f;
  if (diff < -180.0f)
    diff += 360.0f;

  // Apply slew rate limit
  if (diff > st->slew_rate)
    diff = st->slew_rate;
  if (diff < -st->slew_rate)
    diff = -st->slew_rate;

  st->theta_deg += diff;

  // Normalize to 0-360
  if (st->theta_deg < 0)
    st->theta_deg += 360.0f;
  if (st->theta_deg >= 360.0f)
    st->theta_deg -= 360.0f;

  return st->theta_deg;
}
