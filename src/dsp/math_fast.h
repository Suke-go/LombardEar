#ifndef MATH_FAST_H
#define MATH_FAST_H

#include <math.h>

static inline float fast_sqrtf(float x) { return sqrtf(x); }

static inline float fast_absf(float x) { return fabsf(x); }

// Add more optimized versions here later (e.g., approximate rsqrt)

#endif
