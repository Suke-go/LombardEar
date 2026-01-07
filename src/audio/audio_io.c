#include "audio_io.h"
#include <portaudio.h>
#ifdef _WIN32
#include <pa_win_wasapi.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AudioIO {
  PaStream *stream;
  AudioProcessFn callback_fn;
  void *user_data;
  AudioConfig config;
  float *temp_input_buffer;
};

static int paCallback(const void *inputBuffer, void *outputBuffer,
                      unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo *timeInfo,
                      PaStreamCallbackFlags statusFlags, void *userData) {
  AudioIO *aio = (AudioIO *)userData;
  const float *in = (const float *)inputBuffer;
  float *out = (float *)outputBuffer;
  int frames = (int)framesPerBuffer;
  int num_in_ch = aio->config.input_channels;

  (void)timeInfo;
  (void)statusFlags;

  if (in == NULL) {
    // Input underflow? Silence input
    memset(aio->temp_input_buffer, 0, frames * num_in_ch * sizeof(float));
  } else {
    // Remap input channels
    // Logic: channel_map[i] tells which logical channel (L=0, R=1, B=2)
    // the physical channel 'i' maps to.
    // WAIT. The config says "channel_map[3]: physical index -> logical index".
    // e.g. map[0]=1 means Physical Ch 0 is Logical Ch 1 (Right).
    // So we want to construct logical buffer [L][R][B]...
    // But map[phy] = log implies: Logical[log] = Physical[phy].
    // So for Logical Ch 'l', we need to find which 'p' maps to 'l'.
    // Let's invert the map for faster lookup if needed, or loops.
    // Actually, simplest: Dest[frame*3 + log] = Src[frame*N + phy]
    // But map is phy->log.
    // So Dest[frame*3 + map[phy]] = Src[frame*N + phy].

    int map[3];
    memcpy(map, aio->config.channel_map, sizeof(map));

    // Input buffer is interleaved: [P0][P1][P2] [P0][P1][P2] ...
    // We want temp buffer:         [L][R][B]    [L][R][B] ...

    for (int i = 0; i < frames; i++) {
      for (int p = 0; p < num_in_ch; p++) {
        int logical = map[p];
        if (logical >= 0 && logical < 3) {
          aio->temp_input_buffer[i * 3 + logical] = in[i * num_in_ch + p];
        }
      }
    }
  }

  // Call processing function with logical input buffer
  // Output buffer is directly written by the user function
  // Assuming output buffer is already rightly sized for output_channels
  if (aio->callback_fn) {
    return aio->callback_fn(aio->temp_input_buffer, out, frames,
                            aio->user_data);
  }

  // Pass-through silence if no callback
  memset(out, 0, frames * aio->config.output_channels * sizeof(float));
  return paContinue;
}

int audio_open(AudioIO **aio_out, const AudioConfig *cfg, AudioProcessFn fn,
               void *user) {
  PaError err;
  AudioIO *aio = (AudioIO *)malloc(sizeof(AudioIO));
  if (!aio)
    return -1;

  memset(aio, 0, sizeof(AudioIO));
  aio->config = *cfg;
  aio->callback_fn = fn;
  aio->user_data = user;

  // Allocate temp buffer for logical input (always 3 channels internal)
  aio->temp_input_buffer =
      (float *)malloc(cfg->frames_per_buffer * 3 * sizeof(float));
  if (!aio->temp_input_buffer) {
    free(aio);
    return -1;
  }

  err = Pa_Initialize();
  if (err != paNoError) {
    fprintf(stderr, "Pa_Initialize failed: %s\n", Pa_GetErrorText(err));
    free(aio->temp_input_buffer);
    free(aio);
    return -1;
  }

  // Determine requested Host API
  PaHostApiTypeId requestedType = paInDevelopment;
  switch (cfg->backend) {
  case AUDIO_BACKEND_WASAPI_EXCLUSIVE:
  case AUDIO_BACKEND_WASAPI_SHARED:
    requestedType = paWASAPI;
    break;
  case AUDIO_BACKEND_ASIO:
    requestedType = paASIO;
    break;
  case AUDIO_BACKEND_ALSA:
    requestedType = paALSA;
    break;
  case AUDIO_BACKEND_JACK:
    requestedType = paJACK;
    break;
  default:
    requestedType = paInDevelopment;
    break;
  }

  // Find Host API Index if requested
  int hostApiIndex = -1;
  if (requestedType != paInDevelopment) {
    int count = Pa_GetHostApiCount();
    for (int i = 0; i < count; i++) {
      const PaHostApiInfo *info = Pa_GetHostApiInfo(i);
      if (info && info->type == requestedType) {
        hostApiIndex = i;
        break;
      }
    }
    if (hostApiIndex == -1) {
      fprintf(stderr,
              "Warning: Requested Audio Backend not found. Using default.\n");
    }
  }

  // Resolve Devices
  int inputDevice = cfg->input_device_id;
  int outputDevice = cfg->output_device_id;

  if (hostApiIndex != -1) {
    const PaHostApiInfo *info = Pa_GetHostApiInfo(hostApiIndex);
    if (inputDevice == -1)
      inputDevice = info->defaultInputDevice;
    if (outputDevice == -1)
      outputDevice = info->defaultOutputDevice;
  } else {
    if (inputDevice == -1)
      inputDevice = Pa_GetDefaultInputDevice();
    if (outputDevice == -1)
      outputDevice = Pa_GetDefaultOutputDevice();
  }

  PaStreamParameters inputParams = {0};
  inputParams.device = inputDevice;
  inputParams.channelCount = cfg->input_channels;
  inputParams.sampleFormat = paFloat32;
  if (inputDevice != paNoDevice) {
    inputParams.suggestedLatency =
        Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;
  }
  inputParams.hostApiSpecificStreamInfo = NULL;

  PaStreamParameters outputParams = {0};
  outputParams.device = outputDevice;
  outputParams.channelCount = cfg->output_channels;
  outputParams.sampleFormat = paFloat32;
  if (outputDevice != paNoDevice) {
    outputParams.suggestedLatency =
        Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
  }
  outputParams.hostApiSpecificStreamInfo = NULL;

#ifdef _WIN32
  PaWasapiStreamInfo wasapiInfo = {0};
  if (cfg->backend == AUDIO_BACKEND_WASAPI_EXCLUSIVE) {
    // Check if output device is WASAPI
    const PaDeviceInfo *dInfo = Pa_GetDeviceInfo(outputDevice);
    if (dInfo) {
      const PaHostApiInfo *hInfo = Pa_GetHostApiInfo(dInfo->hostApi);
      if (hInfo && hInfo->type == paWASAPI) {
        wasapiInfo.size = sizeof(PaWasapiStreamInfo);
        wasapiInfo.hostApiType = paWASAPI;
        wasapiInfo.version = 1;
        wasapiInfo.flags = paWinWasapiExclusive;
        outputParams.hostApiSpecificStreamInfo = &wasapiInfo;

        // Apply to input if also WASAPI
        const PaDeviceInfo *dInfoIn = Pa_GetDeviceInfo(inputDevice);
        if (dInfoIn) {
          const PaHostApiInfo *hInfoIn = Pa_GetHostApiInfo(dInfoIn->hostApi);
          if (hInfoIn && hInfoIn->type == paWASAPI) {
            inputParams.hostApiSpecificStreamInfo = &wasapiInfo;
          }
        }
      }
    }
  }
#endif

  err = Pa_OpenStream(&aio->stream, &inputParams, &outputParams,
                      cfg->sample_rate, cfg->frames_per_buffer,
                      paClipOff, // We handle clipping/limiting in DSP if needed
                      paCallback, aio);

  if (err != paNoError) {
    fprintf(stderr, "Pa_OpenStream failed: %s\n", Pa_GetErrorText(err));
    Pa_Terminate();
    free(aio->temp_input_buffer);
    free(aio);
    return -1;
  }

  *aio_out = aio;
  return 0;
}

int audio_start(AudioIO *aio) {
  if (!aio || !aio->stream)
    return -1;
  PaError err = Pa_StartStream(aio->stream);
  if (err != paNoError) {
    fprintf(stderr, "Pa_StartStream failed: %s\n", Pa_GetErrorText(err));
    return -1;
  }
  return 0;
}

int audio_stop(AudioIO *aio) {
  if (!aio || !aio->stream)
    return -1;
  PaError err = Pa_StopStream(aio->stream);
  if (err != paNoError)
    return -1;
  return 0;
}

void audio_close(AudioIO *aio) {
  if (!aio)
    return;
  if (aio->stream) {
    Pa_CloseStream(aio->stream);
  }
  Pa_Terminate();
  if (aio->temp_input_buffer)
    free(aio->temp_input_buffer);
  free(aio);
}

int audio_get_devices(AudioDeviceInfo *devices, int max_devices) {
  PaError err = Pa_Initialize();
  if (err != paNoError) {
    fprintf(stderr, "Pa_Initialize failed: %s\n", Pa_GetErrorText(err));
    return 0;
  }

  int numDevices = Pa_GetDeviceCount();
  if (numDevices < 0) {
    fprintf(stderr, "Pa_GetDeviceCount failed: %s\n",
            Pa_GetErrorText(numDevices));
    Pa_Terminate();
    return 0;
  }

  int count = 0;
  for (int i = 0; i < numDevices && count < max_devices; i++) {
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
    if (deviceInfo) {
      devices[count].id = i;
      strncpy(devices[count].name, deviceInfo->name,
              sizeof(devices[count].name) - 1);
      devices[count].name[sizeof(devices[count].name) - 1] = '\0';
      devices[count].max_input_channels = deviceInfo->maxInputChannels;
      devices[count].max_output_channels = deviceInfo->maxOutputChannels;
      count++;
    }
  }

  Pa_Terminate();
  return count;
}

int audio_get_host_apis(AudioHostApiInfo *apis, int max_apis) {
  PaError err = Pa_Initialize();
  if (err != paNoError)
    return 0;

  int count = Pa_GetHostApiCount();
  if (count < 0) {
    Pa_Terminate();
    return 0;
  }

  int written = 0;
  for (int i = 0; i < count && written < max_apis; i++) {
    const PaHostApiInfo *info = Pa_GetHostApiInfo(i);
    if (info) {
      apis[written].index = i;
      apis[written].type = info->type;
      strncpy(apis[written].name, info->name, sizeof(apis[written].name) - 1);
      apis[written].name[sizeof(apis[written].name) - 1] = '\0';
      written++;
    }
  }
  Pa_Terminate();
  return written;
}

void audio_print_devices(void) {
  PaError err = Pa_Initialize();
  if (err != paNoError) {
    fprintf(stderr, "Pa_Initialize failed: %s\n", Pa_GetErrorText(err));
    return;
  }

  int numDevices = Pa_GetDeviceCount();
  if (numDevices < 0) {
    fprintf(stderr, "Pa_GetDeviceCount failed: %s\n",
            Pa_GetErrorText(numDevices));
    Pa_Terminate();
    return;
  }

  printf("Available Audio Devices:\n");
  for (int i = 0; i < numDevices; i++) {
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
    if (deviceInfo) {
      const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
      printf("[%d] %s (%s) (In: %d, Out: %d)\n", i, deviceInfo->name,
             hostInfo ? hostInfo->name : "Unknown",
             deviceInfo->maxInputChannels, deviceInfo->maxOutputChannels);
    }
  }

  Pa_Terminate();
}
