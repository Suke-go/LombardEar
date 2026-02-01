#include "fast_math.h"
#include <math.h>

// Global sine LUT
float fast_sin_table[FAST_SIN_TABLE_SIZE];

void fast_math_init(void) {
  const float TWO_PI = 6.28318530718f;
  for (int i = 0; i < FAST_SIN_TABLE_SIZE; i++) {
    fast_sin_table[i] = sinf(TWO_PI * i / FAST_SIN_TABLE_SIZE);
  }
}
