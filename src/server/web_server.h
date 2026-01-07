#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the background web server thread on the specified port.
void server_init(int port);

/// Update the audio RMS levels (safe to call from audio thread).
void server_update_rms(float l, float r, float b, float err);

/// Update the GSC internal parameters (beta, mu).
/// Update the GSC internal parameters (beta, mu).
void server_update_params(float beta, float mu);

/// Get the latest control parameters from the web client.
/// Returns 1 if parameters were updated, 0 otherwise.
int server_get_ctrl_params(float *alpha, float *leak_lambda, float *mu_max);

/// Get DSP module parameters. Returns 1 if updated.
int server_get_dsp_params(int *aec_on, int *agc_on, int *ng_on,
                          float *agc_target, float *ng_thresh);

/// Set the list of available output devices (called once at startup).
/// devices: JSON string of device array, e.g. [{"id":0,"name":"Speakers"},...]
void server_set_device_list(const char *devices_json);

/// Check if a device change was requested. Returns 1 if yes, with new_device_id
/// set.
int server_get_pending_output_device(int *new_device_id);

#ifdef __cplusplus
}
#endif

#endif
