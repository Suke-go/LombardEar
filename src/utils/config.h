#ifndef CONFIG_H
#define CONFIG_H

#include "../audio/audio_io.h"

#ifdef __cplusplus
extern "C" {
#endif

// Load configuration from JSON file.
// Returns 0 on success, -1 on error.
int config_load(const char *filename, AudioConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H
