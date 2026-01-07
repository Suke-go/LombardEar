#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Platform abstraction layer for LombardEar.
 * Provides cross-platform APIs for:
 * - Non-blocking keyboard input
 * - High-resolution timing
 * - Sleep functions
 */

// Initialize platform subsystem (call once at startup)
int platform_init(void);

// Cleanup platform subsystem
void platform_cleanup(void);

// Non-blocking keyboard check (returns non-zero if key available)
int platform_kbhit(void);

// Get character from keyboard (blocking)
int platform_getch(void);

// Sleep for specified milliseconds
void platform_sleep_ms(int ms);

// Get high-resolution time in microseconds (for profiling)
double platform_time_us(void);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_H
