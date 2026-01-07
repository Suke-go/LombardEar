/**
 * POSIX/Unix implementation of platform abstraction layer.
 * Compatible with Linux and macOS.
 */

#if !defined(_WIN32)

#include "platform.h"
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

static struct termios g_orig_termios;
static int g_termios_saved = 0;

int platform_init(void) {
  // Set terminal to raw mode for non-blocking input
  if (!g_termios_saved && isatty(STDIN_FILENO)) {
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    g_termios_saved = 1;

    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    raw.c_cc[VMIN] = 0;              // Non-blocking
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  }
  return 0;
}

void platform_cleanup(void) {
  // Restore original terminal settings
  if (g_termios_saved) {
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
    g_termios_saved = 0;
  }
}

int platform_kbhit(void) {
  struct timeval tv = {0, 0};
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

int platform_getch(void) {
  int ch = -1;
  if (read(STDIN_FILENO, &ch, 1) == 1) {
    return ch;
  }
  return -1;
}

void platform_sleep_ms(int ms) { usleep(ms * 1000); }

double platform_time_us(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec * 1000000.0 + (double)tv.tv_usec;
}

#endif // !_WIN32
