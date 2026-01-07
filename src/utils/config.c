#include "config.h"
#include "../audio/audio_io.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int config_load(const char *filename, AudioConfig *cfg) {
  FILE *f = fopen(filename, "rb");
  if (!f)
    return -1;

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *data = (char *)malloc(len + 1);
  if (!data) {
    fclose(f);
    return -1;
  }

  size_t read_bytes = fread(data, 1, len, f);
  if (read_bytes != (size_t)len) {
    // Handle short read (though unexpected for file)
    // For simplicity, proceed if >0, or fail
  }
  data[read_bytes] = '\0';
  fclose(f);

  cJSON *json = cJSON_Parse(data);
  free(data);
  if (!json)
    return -1;

  // Safety check: verify parse result
  if (cJSON_IsInvalid(json)) {
    cJSON_Delete(json);
    return -1;
  }

  cJSON *audio_obj = cJSON_GetObjectItem(json, "audio");
  if (!audio_obj) {
    // Fallback: try root level for backward compatibility or flatness
    audio_obj = json;
  }

  cJSON *item = cJSON_GetObjectItem(audio_obj, "input_device_id");
  if (cJSON_IsNumber(item))
    cfg->input_device_id = item->valueint;

  item = cJSON_GetObjectItem(audio_obj, "output_device_id");
  if (cJSON_IsNumber(item))
    cfg->output_device_id = item->valueint;

  item = cJSON_GetObjectItem(audio_obj, "frames_per_buffer");
  if (cJSON_IsNumber(item))
    cfg->frames_per_buffer = item->valueint;

  item = cJSON_GetObjectItem(audio_obj, "sample_rate");
  if (cJSON_IsNumber(item))
    cfg->sample_rate = item->valueint;

  item = cJSON_GetObjectItem(audio_obj, "input_channels");
  if (cJSON_IsNumber(item))
    cfg->input_channels = item->valueint;

  item = cJSON_GetObjectItem(audio_obj, "output_channels");
  if (cJSON_IsNumber(item))
    cfg->output_channels = item->valueint;

  cJSON *map = cJSON_GetObjectItem(audio_obj, "channel_map");
  if (cJSON_IsArray(map) && cJSON_GetArraySize(map) >= 3) {
    for (int i = 0; i < 3; i++) {
      cJSON *val = cJSON_GetArrayItem(map, i);
      if (cJSON_IsNumber(val)) {
        cfg->channel_map[i] = val->valueint;
      }
    }
  }

  item = cJSON_GetObjectItem(audio_obj, "backend");
  if (cJSON_IsNumber(item)) {
    cfg->backend = (AudioBackend)item->valueint;
  } else if (cJSON_IsString(item)) {
    const char *s = item->valuestring;
    if (strcmp(s, "wasapi_shared") == 0)
      cfg->backend = AUDIO_BACKEND_WASAPI_SHARED;
    else if (strcmp(s, "wasapi_exclusive") == 0)
      cfg->backend = AUDIO_BACKEND_WASAPI_EXCLUSIVE;
    else if (strcmp(s, "asio") == 0)
      cfg->backend = AUDIO_BACKEND_ASIO;
    else if (strcmp(s, "alsa") == 0)
      cfg->backend = AUDIO_BACKEND_ALSA;
    else if (strcmp(s, "jack") == 0)
      cfg->backend = AUDIO_BACKEND_JACK;
    else
      cfg->backend = AUDIO_BACKEND_DEFAULT;
  }

  cJSON_Delete(json);
  return 0;
}
