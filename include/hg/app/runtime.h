#ifndef HG_APP_RUNTIME_H
#define HG_APP_RUNTIME_H

#include "hg/diagnostics.h"
#include "hg/services/sound.h"
#include "hg/types.h"

typedef struct {
    hg_light_snapshot_t light;
    hg_indoor_snapshot_t indoor;
    uint32_t policy_generation;
    uint32_t updated_ms;
} hg_sensor_snapshot_t;

typedef struct {
    hg_wifi_snapshot_t wifi;
    hg_weather_snapshot_t weather;
    uint32_t updated_ms;
    bool ready;
    bool stale;
} hg_network_snapshot_t;

/* UI task -> audio worker. The generation is the effects-policy generation. */
typedef struct {
    uint32_t generation;
    hg_sound_id_t sound;
    uint8_t alarm_volume_pct;
} hg_audio_command_t;

/* Audio worker -> UI task. `active` describes the output for `generation`. */
typedef struct {
    uint32_t generation;
    bool active;
    uint32_t underruns;
    uint32_t write_errors;
    uint32_t updated_ms;
#ifdef HG_RUNTIME_TEST
    uint32_t rendered_frames;
    uint32_t drain_frames;
    uint32_t start_attempts;
#endif
} hg_audio_status_t;

typedef struct {
    hg_timing_stats_t capture_gap;
    hg_timing_stats_t input_age;
    hg_timing_stats_t ui_gap;
    hg_timing_stats_t ui_work;
    uint32_t input_overflows;
    uint32_t expired_inputs;
    uint32_t sensor_max_us;
    uint32_t network_max_us;
    /* input, sensor, network, audio (audio is 0 when no speaker) */
    uint32_t stack_free_bytes[4];
    uint32_t input_queue_high_water;
    /* Cumulative since boot; reset_diagnostics keeps them. */
    uint32_t audio_underruns;
    uint32_t audio_write_errors;
    bool touch_pressed;
    bool encoder_pressed;
    bool input_fault;
} hg_runtime_diagnostics_t;

bool hg_runtime_start(void);
const char *hg_runtime_fault(void);
void hg_runtime_set_navigation_allowed(bool allowed);
uint32_t hg_runtime_input_generation(void);
bool hg_runtime_take_input(hg_input_event_t *out);
bool hg_runtime_take_input(hg_input_event_t *out, uint32_t now_ms);
void hg_runtime_get_input_state(hg_input_state_t *out);
uint32_t hg_runtime_set_indoor_paused(bool paused);
void hg_runtime_get_sensors(hg_sensor_snapshot_t *out);
void hg_runtime_get_network(hg_network_snapshot_t *out);
/* The audio worker exists only when the speaker initialized before hg_runtime_start(). */
bool hg_runtime_audio_running(void);
void hg_runtime_set_audio_command(const hg_audio_command_t *command);
bool hg_runtime_get_audio_status(hg_audio_status_t *out);
void hg_runtime_record_ui(uint32_t started_us, uint32_t elapsed_us, bool sliding);
void hg_runtime_get_diagnostics(hg_runtime_diagnostics_t *out);
void hg_runtime_reset_diagnostics(void);

#ifdef HG_RUNTIME_TEST
void hg_runtime_test_fail_worker(int index);
void hg_runtime_test_load(uint32_t sensor_delay_ms, uint32_t network_delay_ms,
                          uint32_t network_busy_ms);
bool hg_runtime_test_input(const hg_input_event_t *event);
void hg_runtime_test_manual_input(bool manual);
void hg_runtime_test_audio_failure(bool start, bool write);
void hg_runtime_test_sensor_cache(const hg_sensor_snapshot_t *snapshot);
#endif

#endif
