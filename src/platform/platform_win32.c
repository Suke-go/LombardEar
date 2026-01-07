/**
 * Windows implementation of platform abstraction layer.
 */

#ifdef _WIN32

#include "platform.h"
#include <conio.h>
#include <windows.h>

static LARGE_INTEGER g_frequency;
static int g_timer_initialized = 0;

int platform_init(void) {
  if (!g_timer_initialized) {
    QueryPerformanceFrequency(&g_frequency);
    g_timer_initialized = 1;
  }
  return 0;
}

void platform_cleanup(void) {
  // Nothing to clean up on Windows
}

int platform_kbhit(void) { return _kbhit(); }

int platform_getch(void) { return _getch(); }

void platform_sleep_ms(int ms) { Sleep((DWORD)ms); }

double platform_time_us(void) {
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return (double)counter.QuadPart * 1000000.0 / (double)g_frequency.QuadPart;
}

#endif // _WIN32
