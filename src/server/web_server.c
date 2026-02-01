#include "web_server.h"
#include "../third_party/mongoose/mongoose.h"
#include <process.h> /* for _beginthread on Windows */
#include <stdio.h>
// #include <stdlib.h>
#include <string.h>

// Shared stats (volatile for simple thread-safe "latest value" semantics)
static volatile float s_rms_l = 0.0f;
static volatile float s_rms_r = 0.0f;
static volatile float s_rms_b = 0.0f;
static volatile float s_rms_err = 0.0f;
static volatile float s_beta = 0.0f;
static volatile float s_mu = 0.0f;

// Shared control params (initialized to defaults, main.c should init these too
// if needed)
static volatile float s_ctrl_alpha = 0.05f;
static volatile float s_ctrl_leak_lambda = 0.0001f;
static volatile float s_ctrl_mu_max = 0.05f;
static volatile int s_ctrl_updated = 0; // Flag to indicate new params available

// DSP Params
static volatile int s_dsp_aec_on = 1;
static volatile int s_dsp_agc_on = 0;
static volatile int s_dsp_ng_on = 0;
static volatile float s_dsp_agc_target = -20.0f;
static volatile float s_dsp_ng_thresh = -50.0f;

// ... existing code ...

// Called by main thread to get latest control params
// Returns 1 if updated, 0 otherwise
// (Removed duplicates)
static char s_device_list_json[4096] = "[]";

// Pending output device change
static volatile int s_pending_output_device = -1; // -1 means no change pending

// Jitter buffer statistics (from audio thread)
static volatile float s_jitter_delay_ms = 0.0f;
static volatile float s_jitter_mean_ms = 0.0f;
static volatile float s_jitter_std_ms = 0.0f;
static volatile float s_jitter_fill = 0.0f;

// Phase alignment offsets (from DSP thread)
#define MAX_PHASE_CHANNELS 4
static volatile float s_phase_offsets[MAX_PHASE_CHANNELS] = {0};
static volatile int s_phase_num_channels = 0;

void server_update_rms(float l, float r, float b, float err) {
  s_rms_l = l;
  s_rms_r = r;
  s_rms_b = b;
  s_rms_err = err;
}

void server_update_params(float beta, float mu) {
  s_beta = beta;
  s_mu = mu;
}

// Called by main thread to get latest control params
// Returns 1 if updated, 0 otherwise
int server_get_ctrl_params(float *alpha, float *leak_lambda, float *mu_max) {
  // We return current values. Flag handling is tricky if split.
  // Let's just return values. Main loop checks optimization flag if it wants,
  // but for 3 floats it's cheap to just copy.
  *alpha = s_ctrl_alpha;
  *leak_lambda = s_ctrl_leak_lambda;
  *mu_max = s_ctrl_mu_max;
  return s_ctrl_updated;
}

int server_get_dsp_params(int *aec_on, int *agc_on, int *ng_on,
                          float *agc_target, float *ng_thresh) {
  *aec_on = s_dsp_aec_on;
  *agc_on = s_dsp_agc_on;
  *ng_on = s_dsp_ng_on;
  *agc_target = s_dsp_agc_target;
  *ng_thresh = s_dsp_ng_thresh;

  int ret = s_ctrl_updated;
  s_ctrl_updated = 0; // Clear it here
  return ret;
}

void server_set_device_list(const char *devices_json) {
  strncpy(s_device_list_json, devices_json, sizeof(s_device_list_json) - 1);
  s_device_list_json[sizeof(s_device_list_json) - 1] = '\0';
}

int server_get_pending_output_device(int *new_device_id) {
  if (s_pending_output_device >= 0) {
    *new_device_id = s_pending_output_device;
    s_pending_output_device = -1;
    return 1;
  }
  return 0;
}

void server_update_jitter_stats(float delay_ms, float jitter_mean_ms,
                                float jitter_std_ms, float fill_ratio) {
  s_jitter_delay_ms = delay_ms;
  s_jitter_mean_ms = jitter_mean_ms;
  s_jitter_std_ms = jitter_std_ms;
  s_jitter_fill = fill_ratio;
}

void server_update_phase_offsets(const float *offsets, int num_channels) {
  if (num_channels > MAX_PHASE_CHANNELS)
    num_channels = MAX_PHASE_CHANNELS;
  s_phase_num_channels = num_channels;
  for (int i = 0; i < num_channels; i++) {
    s_phase_offsets[i] = offsets[i];
  }
}

static struct mg_mgr mgr;
static int g_port = 8000;

static void broadcast_stats(struct mg_mgr *m) {
  char json[1024];

  // Build phase offsets array string
  char phase_str[128] = "[]";
  if (s_phase_num_channels > 0) {
    int pos = 0;
    pos += snprintf(phase_str + pos, sizeof(phase_str) - pos, "[");
    for (int i = 0; i < s_phase_num_channels; i++) {
      pos += snprintf(phase_str + pos, sizeof(phase_str) - pos, "%.2f%s",
                      s_phase_offsets[i],
                      (i < s_phase_num_channels - 1) ? "," : "");
    }
    snprintf(phase_str + pos, sizeof(phase_str) - pos, "]");
  }

  // Extended JSON with jitter and phase stats
  int len = snprintf(json, sizeof(json),
                     "{\"l\": %.4f, \"r\": %.4f, \"b\": %.4f, \"e\": %.4f, "
                     "\"beta\": %.4f, \"mu\": %.6f, "
                     "\"jitter\": {\"delay\": %.1f, \"mean\": %.2f, \"std\": "
                     "%.2f, \"fill\": %.2f}, "
                     "\"phase\": %s}",
                     s_rms_l, s_rms_r, s_rms_b, s_rms_err, s_beta, s_mu,
                     s_jitter_delay_ms, s_jitter_mean_ms, s_jitter_std_ms,
                     s_jitter_fill, phase_str);

  if (len > 0) {
    for (struct mg_connection *c = m->conns; c; c = c->next) {
      if (c->is_websocket) {
        mg_ws_send(c, json, len, WEBSOCKET_OP_TEXT);
      }
    }
  }
}

// Mongoose v7.20 callback signature: 3 args
static void fn(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
      mg_ws_upgrade(c, hm, NULL);
    } else {
      // Serve static files from "web" directory
      struct mg_http_serve_opts opts = {.root_dir = "web"};
      mg_http_serve_dir(c, hm, &opts);
    }
  } else if (ev == MG_EV_WS_OPEN) {
    // Send device list to new WebSocket client
    char msg[4200];
    int len = snprintf(msg, sizeof(msg), "{\"type\":\"devices\",\"list\":%s}",
                       s_device_list_json);
    if (len > 0) {
      mg_ws_send(c, msg, len, WEBSOCKET_OP_TEXT);
    }
  } else if (ev == MG_EV_WS_MSG) {
    struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
    // Parse JSON command: {"alpha": 0.01, "leak": 0.001, "mu_max": 0.1}
    // We use mg_json_get_num for simple parsing if available, or just parsing
    // manually/cJSON. Since we don't assume cJSON headers are easy to include
    // here without path issues, let's use Mongoose's helper if possible or
    // simple string searching for now to strictly avoid dependency hell in this
    // file. Actually, we can use mg_json_get_num.

    double v;
    int updated = 0;
    if (mg_json_get_num(wm->data, "$.alpha", &v)) {
      s_ctrl_alpha = (float)v;
      updated = 1;
    }
    if (mg_json_get_num(wm->data, "$.leak", &v)) {
      s_ctrl_leak_lambda = (float)v;
      updated = 1;
    }
    if (mg_json_get_num(wm->data, "$.mu_max", &v)) {
      s_ctrl_mu_max = (float)v;
      updated = 1;
    }

    // DSP Controls
    if (mg_json_get_num(wm->data, "$.aec_on", &v)) {
      s_dsp_aec_on = (int)v;
      updated = 1;
    }
    if (mg_json_get_num(wm->data, "$.agc_on", &v)) {
      s_dsp_agc_on = (int)v;
      updated = 1;
    }
    if (mg_json_get_num(wm->data, "$.ng_on", &v)) {
      s_dsp_ng_on = (int)v;
      updated = 1;
    }
    if (mg_json_get_num(wm->data, "$.agc_target", &v)) {
      s_dsp_agc_target = (float)v;
      updated = 1;
    }
    if (mg_json_get_num(wm->data, "$.ng_thresh", &v)) {
      s_dsp_ng_thresh = (float)v;
      updated = 1;
    }

    if (updated) {
      s_ctrl_updated = 1;
    }

    // Check for set_output_device command
    if (mg_json_get_num(wm->data, "$.set_output_device", &v)) {
      s_pending_output_device = (int)v;
      printf("Server: Output device change requested: %d\n",
             s_pending_output_device);
    }
  }
}

static void server_thread(void *arg) {
  (void)arg;
  mg_mgr_init(&mgr);

  char addr[64];
  snprintf(addr, sizeof(addr), "http://0.0.0.0:%d", g_port);

  // MG_INFO(("Starting Mongoose server on %s", addr));
  mg_http_listen(&mgr, addr, fn, NULL);

  while (1) {
    mg_mgr_poll(&mgr, 40); // 40ms poll ~ 25fps response
    broadcast_stats(&mgr); // Broadcast every loop (approx 25Hz)
  }

  mg_mgr_free(&mgr);
}

void server_init(int port) {
  g_port = port;
  if (_beginthread(server_thread, 0, NULL) == -1L) {
    fprintf(stderr, "Failed to create server thread\n");
  }
}
