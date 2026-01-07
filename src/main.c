#include "audio/audio_io.h"
#include "dsp/aec.h"
#include "dsp/agc.h"
#include "dsp/gsc.h"
#include "dsp/noise_gate.h"
#include "platform/platform.h"
#include "server/web_server.h"
#include "utils/config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Context wrapper for callback
typedef struct {
  GscState st;
  GscConfig cfg;
  float *gsc_mem;

  // DSP States
  AecState aec;
  AgcState agc;
  NoiseGateState ng;

  // DSP Buffers
  float *aec_mem;

  // Controls
  int aec_on;
  int agc_on;
  int ng_on;

  // History
  float last_ref_sample; // For AEC loopback
} AppContext;

// GSC Processing Callback
int process_audio(const float *in, float *out, int frames, void *user) {
  AppContext *ctx = (AppContext *)user;

  // Check for control updates
  float ctrl_alpha, ctrl_leak, ctrl_mu_max;

  // We check both getters
  int params_updated = 0;
  if (server_get_ctrl_params(&ctrl_alpha, &ctrl_leak, &ctrl_mu_max)) {
    ctx->cfg.alpha = ctrl_alpha;
    ctx->cfg.leak_lambda = ctrl_leak;
    ctx->cfg.mu_max = ctrl_mu_max;
    params_updated = 1;
  }

  int aec_on, agc_on, ng_on;
  float agc_target, ng_thresh;
  if (server_get_dsp_params(&aec_on, &agc_on, &ng_on, &agc_target,
                            &ng_thresh)) {
    ctx->aec_on = aec_on;
    ctx->agc_on = agc_on;
    ctx->ng_on = ng_on;

    // Update internal params if needed (re-init or setters? For now re-init is
    // heavy, maybe just direct struct access or specific setters) Since our
    // simple DSP libs expose structs:
    if (agc_on) {
      // Re-calc target RMS from dB?
      // agc_target is in dB.
      ctx->agc.target_rms = powf(10.0f, agc_target / 20.0f);
    }
    if (ng_on) {
      ctx->ng.threshold_linear = powf(10.0f, ng_thresh / 20.0f);
    }
    params_updated = 1;
  }

  if (params_updated) {
    // Logging could be verbose here, minimizing
  }

  float sum_l = 0, sum_r = 0, sum_b = 0, sum_e = 0;

  // Profiling
  double start_us = platform_time_us();

  for (int i = 0; i < frames; i++) {
    float xL = in[i * 3 + 0];
    float xR = in[i * 3 + 1];
    float xB = in[i * 3 + 2];

    // 1. GSC (Beamforming)
    float y = gsc_process_sample(&ctx->st, &ctx->cfg, xL, xR, xB);

    // 2. AEC (Remove echo of PREVIOUS output from CURRENT input)
    // Ref: ctx->last_ref_sample
    if (ctx->aec_on) {
      // AEC returns the error signal (echo removed)
      y = aec_process(&ctx->aec, y, ctx->last_ref_sample);
    }

    // 3. AGC
    if (ctx->agc_on) {
      y = agc_process(&ctx->agc, y);
    }

    // 4. Noise Gate
    if (ctx->ng_on) {
      y = noise_gate_process(&ctx->ng, y);
    }

    // Stats accumulation (using y as 'e' - enhanced)
    sum_l += xL * xL;
    sum_r += xR * xR;
    sum_b += xB * xB;
    sum_e += y * y;

    // Output
    out[i * 2 + 0] = y;
    out[i * 2 + 1] = y;

    // Update reference for next sample
    ctx->last_ref_sample = y;
  }

  double end_us = platform_time_us();
  double elapsed_us = end_us - start_us;

  static double max_us = 0;
  static long long call_count = 0;

  if (elapsed_us > max_us)
    max_us = elapsed_us;
  call_count++;

  if (call_count % 100 == 0) {
    printf("DSP Load: %.2f us / block (Max: %.2f us)\n", elapsed_us, max_us);
    max_us = 0;
  }

  // Update Server Stats
  if (frames > 0) {
    server_update_rms(sqrtf(sum_l / frames), sqrtf(sum_r / frames),
                      sqrtf(sum_b / frames), sqrtf(sum_e / frames));
    server_update_params(ctx->st.beta, ctx->st.last_mu);
  }

  return 0; // Continue
}

int main(int argc, char **argv) {
  // Initialize platform subsystem
  platform_init();
  // Check for --list-devices
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--list-devices") == 0) {
      audio_print_devices();
      return 0;
    }
  }

  AudioConfig audio_cfg = {
      .sample_rate = 16000, // Reduced to 16k for initial GSC testing implies
                            // lower CPU load
      .input_channels = 3,
      .output_channels = 2,
      .frames_per_buffer =
          64, // Optimization: Reduced to 4ms (64 samples @ 16kHz)
      .input_device_id = -1,
      .output_device_id = -1,
      .channel_map = {0, 1, 2} // Default: Phy0->L, Phy1->R, Phy2->B
  };

  printf("LombardEar Phase 4: GSC Integration\n");
  printf("Loading audio config/default.json...\n");
  if (config_load("config/default.json", &audio_cfg) == 0) {
    printf("Audio Config loaded. Devices: In=%d, Out=%d\n",
           audio_cfg.input_device_id, audio_cfg.output_device_id);
  } else {
    printf("Failed to load audio config. Using defaults.\n");
  }

  // FORCE 16kHz for GSC consistency for now (unless we made GSC rate-agnostic?
  // GSC is mostly rate agnostic but filters might need tuning).
  // Let's respect what's in local variable if config didn't override, or
  // override it to 16000 if needed. Ideally config.json should specify it. For
  // now, let's overwrite to ensure stability.
  audio_cfg.sample_rate = 16000;
  audio_cfg.frames_per_buffer =
      480; // Relaxed to 480 (30ms) for stability check

  printf("Initializing GSC...\n");
  AppContext ctx;
  ctx.cfg.M = 64;
  ctx.cfg.alpha = 0.01f; // Much slower adaptation
  ctx.cfg.eps = 1e-6f;
  ctx.cfg.mu_max = 0.01f; // Much conservative max step size
  ctx.cfg.eta_max = 0.001f;
  ctx.cfg.leak_lambda = 0.0001f;
  ctx.cfg.g_lo = 0.1f;
  ctx.cfg.g_hi = 0.3f;
  ctx.cfg.beta_min = -2.0f;
  ctx.cfg.beta_max = 2.0f;

  size_t mem_size = 4 * ctx.cfg.M * sizeof(float);
  ctx.gsc_mem = malloc(mem_size);
  if (!ctx.gsc_mem) {
    fprintf(stderr, "Failed to allocate GSC memory\n");
    return 1;
  }

  if (gsc_init(&ctx.st, &ctx.cfg, ctx.gsc_mem, mem_size) != 0) {
    fprintf(stderr, "Failed to init GSC state\n");
    free(ctx.gsc_mem);
    return 1;
  }

  // Init AEC
  // Filter length 512 samples (~32ms @ 16kHz) to cover system loopback latency
  // Need to fine-tune M based on Measured Loopback Latency.
  // For WASAPI Exclusive 4ms buffer, latency might be ~10ms. 512 is plenty.
  int aec_M = 1024;
  size_t aec_mem_size = 2 * aec_M * sizeof(float);
  ctx.aec_mem = malloc(aec_mem_size);
  if (!ctx.aec_mem) {
    fprintf(stderr, "Failed to allocate AEC memory\n");
    free(ctx.gsc_mem);
    return 1;
  }
  aec_init(&ctx.aec, aec_M, ctx.aec_mem, aec_mem_size);

  // Init AGC
  // target -20dB, attack 10ms, release 500ms, max +30dB
  agc_init(&ctx.agc, -30.0f, 10.0f, 500.0f, 20.0f, audio_cfg.sample_rate);

  // Init Noise Gate
  // thresh -50dB, hold 200ms, release 100ms
  noise_gate_init(&ctx.ng, -50.0f, 200.0f, 100.0f, audio_cfg.sample_rate);

  // Defaults
  ctx.aec_on = 0; // Off by default to isolate GSC stability
  ctx.agc_on = 0; // Off by default
  ctx.ng_on = 0;  // Off by default
  ctx.last_ref_sample = 0.0f;

  printf("Initializing 3ch Input -> 2ch Output with GSC + DSP Chain...\n");

// Start Web Server
#ifdef LE_WITH_WEBSOCKETS
  // Build device list JSON for web UI
  {
    AudioDeviceInfo devs[64];
    int n_devs = audio_get_devices(devs, 64);
    char dev_json[4096] = "[";
    int pos = 1;
    for (int i = 0; i < n_devs; i++) {
      if (devs[i].max_output_channels > 0) {
        if (pos > 1)
          pos += snprintf(dev_json + pos, sizeof(dev_json) - pos, ",");
        pos +=
            snprintf(dev_json + pos, sizeof(dev_json) - pos,
                     "{\"id\":%d,\"name\":\"%s\"}", devs[i].id, devs[i].name);
      }
    }
    snprintf(dev_json + pos, sizeof(dev_json) - pos, "]");
    server_set_device_list(dev_json);
    printf("Registered %d output devices for Web UI\n", n_devs);
  }
  server_init(8000);
#endif

  AudioIO *aio = NULL;
  // Pass address of ctx struct as user_data
  if (audio_open(&aio, &audio_cfg, process_audio, &ctx) != 0) {
    fprintf(stderr, "Failed to initialize Audio IO\n");
    free(ctx.gsc_mem);
    return 1;
  }

  printf("Audio initialized. Starting stream...\n");
  if (audio_start(aio) != 0) {
    fprintf(stderr, "Failed to start audio stream\n");
    audio_close(aio);
    free(ctx.gsc_mem);
    return 1;
  }

  printf("Running GSC... Press Enter to quit (or switch device via Web UI).\n");

  // Polling loop for device switching and exit
  int running = 1;
  while (running) {
    // Check for Enter key (non-blocking, cross-platform)
    if (platform_kbhit()) {
      int ch = platform_getch();
      if (ch == '\r' || ch == '\n' || ch == 'q') {
        running = 0;
      }
    }

#ifdef LE_WITH_WEBSOCKETS
    // Check for device change request
    int new_device_id;
    if (server_get_pending_output_device(&new_device_id)) {
      printf("Switching output device to ID %d...\n", new_device_id);
      audio_stop(aio);
      audio_close(aio);
      aio = NULL;

      audio_cfg.output_device_id = new_device_id;

      if (audio_open(&aio, &audio_cfg, process_audio, &ctx) != 0) {
        fprintf(stderr, "Failed to re-open audio with new device\n");
        running = 0;
      } else if (audio_start(aio) != 0) {
        fprintf(stderr, "Failed to start audio on new device\n");
        running = 0;
      } else {
        printf("Audio output switched successfully.\n");
      }
    }
#endif

    platform_sleep_ms(100); // 100ms polling interval
  }

  printf("Stopping...\n");
  if (aio) {
    audio_stop(aio);
    audio_close(aio);
  }
  free(ctx.gsc_mem);
  if (ctx.aec_mem)
    free(ctx.aec_mem);
  platform_cleanup();
  printf("Done.\n");

  return 0;
}
