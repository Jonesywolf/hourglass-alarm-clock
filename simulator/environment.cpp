#include "hg/app/simulator.h"
#include "hg/app/runtime.h"
#include "hg/hal/led_strip.h"
#include "hg/hal/rtc.h"
#include "hg/hal/speaker.h"
#include "hg/log.h"
#include "hg/services/store.h"
#include "hg/services/time.h"
#include "hg/services/wifi.h"

#include <emscripten.h>
#include <stdarg.h>
#include <stdio.h>

static hg_time_snapshot_t clock_value = {true, 10, 8, 0};
static uint32_t clock_started = 0;
static hg_sensor_snapshot_t sensors;
static hg_network_snapshot_t network;
static hg_alarm_snapshot_t saved = {true, false, 7, 0, false, 75, HG_ALARM_SOUND_MALLETS};
static hg_alarm_snapshot_t desired;
static hg_store_status_t store_status;

EM_JS(void, show_error, (const char *text), {
    Module.onError(UTF8ToString(text));
});

void hg_log_init(void) {}
void hg_log_write(const char *tag, const char *format, ...) {
    char text[512];
    int prefix = snprintf(text, sizeof(text), "[%s] ", tag);
    if (prefix < 0 || prefix >= (int)sizeof(text)) {
        show_error("Simulator log formatting failed");
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(text + prefix, sizeof(text) - prefix, format, args);
    va_end(args);
    show_error(text);
}
void hg_log_flush(void) {}
uint32_t hg_log_dropped(void) { return 0; }

void hg_sim_set_time(int hour, int minute, int second, bool valid) {
    clock_value = {valid, hour, minute, second};
    clock_started = hg_sim_now();
}
void hg_time_init(void) {}
void hg_time_get_snapshot(hg_time_snapshot_t *out) {
    *out = clock_value;
    uint32_t seconds = (clock_value.hour_24 * 3600 + clock_value.minute * 60 +
                        clock_value.second + (hg_sim_now() - clock_started) / 1000) % 86400;
    out->hour_24 = seconds / 3600;
    out->minute = seconds / 60 % 60;
    out->second = seconds % 60;
}
void hg_sim_set_environment(int wifi, int status, float lux, float outdoor_c,
                            float indoor_c, float humidity, int aqi) {
    bool valid = status != HG_SAMPLE_WAITING && status != HG_SAMPLE_UNAVAILABLE;
    auto sample_status = (hg_sample_status_t)status;
    sensors.light = {true, lux, hg_sim_now(), HG_SAMPLE_READY};
    sensors.indoor = {valid, indoor_c, humidity, 8.0f, aqi, hg_sim_now(), sample_status};
    network.weather = {valid, outdoor_c, 0, aqi, hg_sim_now(), sample_status};
    network.wifi = {(hg_wifi_state_t)wifi, wifi == HG_WIFI_CONNECTED ? -55 : 0,
                    wifi == HG_WIFI_CONNECTED ? HG_WIFI_BARS_4 : HG_WIFI_BARS_NONE};
    network.ready = true;
    network.stale = sample_status == HG_SAMPLE_STALE;
}
void hg_runtime_get_sensors(hg_sensor_snapshot_t *out) {
    *out = sensors;
    out->updated_ms = hg_sim_now();
    if (out->light.status == HG_SAMPLE_READY) out->light.sampled_ms = hg_sim_now();
    if (out->indoor.status == HG_SAMPLE_READY) out->indoor.sampled_ms = hg_sim_now();
}
void hg_runtime_get_network(hg_network_snapshot_t *out) {
    *out = network;
    out->updated_ms = hg_sim_now();
    if (out->weather.status == HG_SAMPLE_READY) out->weather.sampled_ms = hg_sim_now();
}
const char *hg_wifi_get_target_ssid(void) { return "Preview Wi-Fi"; }

void hg_sim_seed_alarm(const hg_alarm_snapshot_t *alarm) { saved = *alarm; }
void hg_store_init(void) { store_status = {}; desired = saved; }
bool hg_store_load_alarm(hg_alarm_snapshot_t *out) { *out = saved; return true; }
bool hg_store_start_worker(void) { return true; }
void hg_store_request_save_alarm(const hg_alarm_snapshot_t *alarm) {
    desired = *alarm;
    ++store_status.requested_generation;
    store_status.pending = true;
}
void hg_store_set_flash_allowed(bool allowed) {
    if (allowed && store_status.pending) {
        saved = desired;
        store_status.saved_generation = store_status.requested_generation;
        store_status.pending = false;
        ++store_status.successful_saves;
    }
}
void hg_store_get_status(hg_store_status_t *out) { *out = store_status; }
bool hg_rtc_init(void) { return false; }
bool hg_speaker_init(void) { return false; }
bool hg_led_strip_init(void) { return false; }
