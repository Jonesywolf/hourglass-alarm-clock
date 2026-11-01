#include "hg/app/simulator.h"
#include "hg/app/runtime.h"
#include "hg/config.h"
#include "hg/hal/led_strip.h"
#include "hg/hal/rtc.h"
#include "hg/hal/speaker.h"
#include "hg/log.h"
#include "hg/services/air_rating.h"
#include "hg/services/location.h"
#include "hg/services/store.h"
#include "hg/services/time.h"
#include "hg/services/wifi.h"

#include <emscripten.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

static hg_time_snapshot_t clock_value = {true, 10, 8, 0};
static uint32_t clock_started = 0;
static hg_sensor_snapshot_t sensors;
static hg_network_snapshot_t network;
static hg_alarm_snapshot_t saved = {true, false, 7, 0, false, 75, HG_ALARM_SOUND_MALLETS};
static hg_alarm_snapshot_t desired;
static hg_store_status_t store_status;
static hg_unit_system_t units = HG_DEFAULT_UNITS;

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

static hg_sim_weather_t weather_input;
static bool weather_set = false;

void hg_sim_set_time(int hour, int minute, int second, bool valid) {
    clock_value = {valid, hour, minute, second};
    clock_started = hg_sim_now();
    if (weather_set) {
        hg_sim_weather_t again = weather_input;
        hg_sim_set_weather(&again);
    }
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
static const hg_sample_status_t ready_like[] = {
    HG_SAMPLE_WAITING, HG_SAMPLE_READY, HG_SAMPLE_STALE, HG_SAMPLE_READY, HG_SAMPLE_ERROR,
    HG_SAMPLE_ERROR};

static bool sample_valid(int status) {
    return status != HG_SAMPLE_WAITING && status != HG_SAMPLE_UNAVAILABLE;
}

void hg_sim_set_environment(int wifi, float lux) {
    sensors.light = {true, lux, hg_sim_now(), HG_SAMPLE_READY};
    network.wifi = {(hg_wifi_state_t)wifi, wifi == HG_WIFI_CONNECTED ? -55 : 0,
                    wifi == HG_WIFI_CONNECTED ? HG_WIFI_BARS_4 : HG_WIFI_BARS_NONE};
    network.ready = true;
}

static bool wet(int condition) {
    return condition >= HG_WX_DRIZZLE && condition <= HG_WX_HAIL;
}

void hg_sim_set_weather(const hg_sim_weather_t *in) {
    weather_input = *in;
    weather_set = true;
    hg_weather_snapshot_t &w = network.weather;
    w = {};
    w.valid = sample_valid(in->status);
    /* The weather service has no paused/unavailable states: it reports ERROR without data. */
    w.status = ready_like[in->status];
    w.sampled_ms = hg_sim_now();
    hg_time_snapshot_t now;
    hg_time_get_snapshot(&now);
    int minute = now.hour_24 * 60 + now.minute - (w.status == HG_SAMPLE_STALE ? 60 : 4);
    w.updated_minute_of_day = (int16_t)((minute + 1440) % 1440);
    w.temp_c = in->temp_c;
    w.feels_like_c = in->feels_c;
    w.condition = (hg_weather_condition_t)in->condition;
    w.is_day = now.hour_24 >= 6 && now.hour_24 < 20;
    w.humidity_pct = 60;
    w.wind_kph = in->wind_kph;
    w.gust_kph = in->gust_kph;
    w.wind_dir_deg = 240;
    w.high_c = in->high_c;
    w.low_c = in->low_c;
    w.day_condition = w.condition;
    w.day_precip_prob_pct = (uint8_t)in->rain_pct;
    w.day_precip_mm = in->rain_pct >= HG_WEATHER_RAIN_PROB_PCT ? 6.0f : 0.0f;
    w.uv_max = in->uv;
    w.day_gust_max_kph = in->gust_kph;
    bool raining = wet(in->condition);
    bool later = !raining && in->rain_pct >= HG_WEATHER_RAIN_PROB_PCT;
    int rain_from = raining ? 0 : 3;
    w.rain_start_hour = w.rain_end_hour = -1;
    w.hourly_count = HG_WEATHER_HOURS;
    for (int i = 0; i < HG_WEATHER_HOURS; ++i) {
        hg_weather_hour_t &slot = w.hourly[i];
        int hour = (now.hour_24 + i) % 24;
        float phase = (float)(hour - 3) * 2.0f * (float)M_PI / 24.0f;
        bool rainy = (raining || later) && i >= rain_from && i < rain_from + 4;
        slot.hour_24 = (uint8_t)hour;
        slot.is_day = hour >= 6 && hour < 20;
        slot.temp_c = i == 0 ? in->temp_c :
            in->low_c + (in->high_c - in->low_c) * (0.5f - 0.5f * cosf(phase));
        slot.condition = rainy ? (raining ? in->condition : HG_WX_RAIN) :
            (raining || later ? HG_WX_CLOUDY : in->condition);
        slot.precip_prob_pct = rainy ? (uint8_t)(in->rain_pct > 0 ? in->rain_pct : 80) :
            (uint8_t)(i % 3 == 0 ? 10 : 0);
        if (rainy && hour >= now.hour_24) {
            if (w.rain_start_hour < 0) {
                w.rain_start_hour = (int8_t)hour;
            }
            w.rain_end_hour = (int8_t)hour;
        }
    }
    if (later) {
        w.day_condition = HG_WX_RAIN;
    }
    w.air_valid = sample_valid(in->air_status);
    w.air_status = ready_like[in->air_status];
    w.us_aqi = (uint16_t)in->aqi;
    w.aqi_category = (uint8_t)hg_air_us_aqi_category(in->aqi);
    w.air_pm2_5 = NAN;
    if (in->status != HG_SAMPLE_WAITING) {
        snprintf(w.location, sizeof(w.location), "Ottawa");
    }
    network.stale = w.status == HG_SAMPLE_STALE;
}

void hg_sim_set_indoor(int status, float temp_c, float humidity, int co2_ppm, float pm2_5,
                       int voc_index) {
    hg_indoor_snapshot_t &indoor = sensors.indoor;
    indoor = {};
    indoor.valid = sample_valid(status);
    indoor.status = (hg_sample_status_t)status;
    indoor.sampled_ms = hg_sim_now();
    indoor.temp_c = temp_c;
    indoor.humidity = humidity;
    indoor.pm1_0 = indoor.pm4_0 = indoor.pm10 = NAN;
    indoor.pm2_5 = pm2_5;
    indoor.co2_ppm = (uint16_t)co2_ppm;
    indoor.voc_index = (uint16_t)voc_index;
    if (indoor.valid) {
        hg_air_rate_indoor(&indoor);
    }
}

void hg_sim_set_units(int value) { units = (hg_unit_system_t)value; }
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
bool hg_store_load_settings(hg_settings_t *out) { out->units = units; return true; }
void hg_store_request_save_settings(const hg_settings_t *in) { units = in->units; }
bool hg_store_load_location(hg_location_t *out) { *out = {}; return false; }
void hg_store_request_save_location(const hg_location_t *) {}
void hg_location_seed(const hg_location_t *) {}
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
