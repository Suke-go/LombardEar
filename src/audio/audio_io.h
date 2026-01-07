#ifndef AUDIO_IO_H
#define AUDIO_IO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  AUDIO_BACKEND_DEFAULT = 0,
  AUDIO_BACKEND_WASAPI_SHARED,
  AUDIO_BACKEND_WASAPI_EXCLUSIVE, // Windows Low-Latency
  AUDIO_BACKEND_ASIO,             // Professional Audio
  AUDIO_BACKEND_ALSA,             // Linux
  AUDIO_BACKEND_JACK              // Linux Low-Latency
} AudioBackend;

typedef struct {
  int sample_rate;       // 48000 recommended
  int input_channels;    // 3 (L, R, Back)
  int output_channels;   // 1 or 2
  int frames_per_buffer; // 480 (10ms @ 48kHz)
  int input_device_id;   // -1: use default
  int output_device_id;  // -1: use default
  int channel_map[3];    // physical index to logical index mapping {L, R, B}
                         // e.g. {0, 1, 2} means Phy0->L, Phy1->R, Phy2->B
  AudioBackend backend;  // Desired audio backend
} AudioConfig;

typedef struct AudioIO AudioIO;

/*
 * Audio processing callback.
 *
 * in_interleaved: Input buffer [frame0_ch0, frame0_ch1, frame0_ch2,
 * frame1_ch0...] out_interleaved: Output buffer to fill frames: Number of
 * frames to process user: User data pointer passed to audio_open
 *
 * Returns: 0 to continue, non-zero to stop (paComplete/paAbort)
 */
typedef int (*AudioProcessFn)(const float *in_interleaved,
                              float *out_interleaved, int frames, void *user);

int audio_open(AudioIO **aio, const AudioConfig *cfg, AudioProcessFn fn,
               void *user);
int audio_start(AudioIO *aio);
int audio_stop(AudioIO *aio);
void audio_close(AudioIO *aio);

// Device info structure for programmatic access
typedef struct {
  int id;
  char name[128];
  int max_input_channels;
  int max_output_channels;
} AudioDeviceInfo;

// Get list of available audio devices
// Returns number of devices, fills 'devices' array up to 'max_devices'
int audio_get_devices(AudioDeviceInfo *devices, int max_devices);

// Host API info
typedef struct {
  int index;
  int type; // PaHostApiTypeId
  char name[128];
} AudioHostApiInfo;

// Get available Host APIs
int audio_get_host_apis(AudioHostApiInfo *apis, int max_apis);

// Print all available PortAudio devices to stdout
void audio_print_devices(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_IO_H
