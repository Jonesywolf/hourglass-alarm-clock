#include "hg/services/weather.h"

#include <Arduino.h>
#include <WiFi.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hg/config.h"
#include "hg/log.h"
#include "hg/services/air_rating.h"
#include "hg/services/location.h"
#include "http_internal.h"

static hg_weather_snapshot_t snapshot;
static uint32_t forecast_ok_ms = 0;
static uint32_t forecast_attempt_ms = 0;
static uint32_t forecast_retry_ms = HG_WEATHER_RETRY_MIN_MS;
static bool forecast_attempted = false;
static bool forecast_failed = false;
static uint32_t air_ok_ms = 0;
static uint32_t air_attempt_ms = 0;
static uint32_t air_retry_ms = HG_WEATHER_RETRY_MIN_MS;
static bool air_attempted = false;
static bool air_failed = false;
static float fetched_lat = NAN;
static float fetched_lon = NAN;

static hg_weather_condition_t condition_from_wmo(int code) {
    switch (code) {
        case 0: case 1: return HG_WX_CLEAR;
        case 2: return HG_WX_PARTLY_CLOUDY;
        case 3: return HG_WX_CLOUDY;
        case 45: case 48: return HG_WX_FOG;
        case 51: case 53: case 55: return HG_WX_DRIZZLE;
        case 56: case 57: case 66: case 67: return HG_WX_FREEZING_RAIN;
        case 61: case 63: case 80: case 81: return HG_WX_RAIN;
        case 65: case 82: return HG_WX_HEAVY_RAIN;
        case 71: case 73: case 77: case 85: return HG_WX_SNOW;
        case 75: case 86: return HG_WX_HEAVY_SNOW;
        case 95: return HG_WX_THUNDERSTORM;
        case 96: case 99: return HG_WX_HAIL;
        default: return HG_WX_UNKNOWN;
    }
}

/* Open-Meteo local ISO time "YYYY-MM-DDTHH:MM". */
static int iso_field(const char *iso, size_t offset) {
    if (!iso || strlen(iso) < 16) {
        return -1;
    }
    return (iso[offset] - '0') * 10 + (iso[offset + 1] - '0');
}

static bool same_date(const char *a, const char *b) {
    return a && b && strlen(a) >= 10 && strlen(b) >= 10 && strncmp(a, b, 10) == 0;
}

static bool due(bool attempted, bool failed, uint32_t ok_ms, uint32_t attempt_ms,
                uint32_t retry_ms, uint32_t refresh_ms, bool have_data, uint32_t now) {
    if (attempted && failed) {
        return now - attempt_ms >= retry_ms;
    }
    return !have_data || now - ok_ms >= refresh_ms;
}

static uint32_t backoff(uint32_t retry_ms) {
    return retry_ms >= HG_WEATHER_RETRY_MAX_MS / 2 ? HG_WEATHER_RETRY_MAX_MS : retry_ms * 2;
}

static int clamp_int(int value, int low, int high) {
    return value < low ? low : (value > high ? high : value);
}

static bool fetch_forecast(const hg_location_t *location) {
    char url[640];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,apparent_temperature,relative_humidity_2m,is_day,"
             "weather_code,wind_speed_10m,wind_gusts_10m,wind_direction_10m"
             "&hourly=temperature_2m,weather_code,precipitation_probability,is_day"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
             "precipitation_probability_max,precipitation_sum,uv_index_max,wind_gusts_10m_max"
             "&timezone=auto&forecast_days=2&forecast_hours=%d",
             location->latitude, location->longitude, HG_WEATHER_HOURS);
    JsonDocument filter;
    filter["current"] = true;
    filter["hourly"] = true;
    filter["daily"] = true;
    JsonDocument doc;
    if (!hg_http_get_json(url, doc, filter)) {
        return false;
    }
    JsonObject current = doc["current"];
    JsonObject hourly = doc["hourly"];
    JsonObject daily = doc["daily"];
    if (current.isNull() || hourly.isNull() || daily.isNull() ||
        !current["temperature_2m"].is<float>() || !current["weather_code"].is<int>()) {
        HG_ERROR("weather", "Forecast response missing fields\n");
        return false;
    }

    hg_weather_snapshot_t next = snapshot;
    const char *now_iso = current["time"];
    int now_hour = iso_field(now_iso, 11);
    int now_minute = iso_field(now_iso, 14);
    next.updated_minute_of_day = now_hour >= 0 ? (int16_t)(now_hour * 60 + now_minute) : -1;
    next.temp_c = current["temperature_2m"];
    next.feels_like_c = current["apparent_temperature"] | next.temp_c;
    next.humidity_pct = (uint8_t)clamp_int(current["relative_humidity_2m"] | 0, 0, 100);
    next.is_day = (current["is_day"] | 1) != 0;
    next.condition = condition_from_wmo(current["weather_code"]);
    next.wind_kph = current["wind_speed_10m"] | 0.0f;
    next.gust_kph = current["wind_gusts_10m"] | next.wind_kph;
    next.wind_dir_deg = (uint16_t)clamp_int(current["wind_direction_10m"] | 0, 0, 359);

    next.high_c = daily["temperature_2m_max"][0] | next.temp_c;
    next.low_c = daily["temperature_2m_min"][0] | next.temp_c;
    next.day_condition = condition_from_wmo(daily["weather_code"][0] | -1);
    next.day_precip_prob_pct =
        (uint8_t)clamp_int(daily["precipitation_probability_max"][0] | 0, 0, 100);
    next.day_precip_mm = daily["precipitation_sum"][0] | 0.0f;
    next.uv_max = daily["uv_index_max"][0] | 0.0f;
    next.day_gust_max_kph = daily["wind_gusts_10m_max"][0] | next.gust_kph;

    JsonArray times = hourly["time"];
    JsonArray temps = hourly["temperature_2m"];
    JsonArray codes = hourly["weather_code"];
    JsonArray probs = hourly["precipitation_probability"];
    JsonArray days = hourly["is_day"];
    size_t count = times.size();
    if (count > HG_WEATHER_HOURS) {
        count = HG_WEATHER_HOURS;
    }
    next.rain_start_hour = -1;
    next.rain_end_hour = -1;
    bool rain_closed = false;
    next.hourly_count = 0;
    for (size_t i = 0; i < count; ++i) {
        const char *time_iso = times[i];
        int hour = iso_field(time_iso, 11);
        if (hour < 0 || hour > 23) {
            break;
        }
        hg_weather_hour_t &slot = next.hourly[next.hourly_count++];
        slot.hour_24 = (uint8_t)hour;
        slot.temp_c = temps[i] | 0.0f;
        slot.condition = (uint8_t)condition_from_wmo(codes[i] | -1);
        slot.precip_prob_pct = (uint8_t)clamp_int(probs[i] | 0, 0, 100);
        slot.is_day = (days[i] | 1) != 0;
        /* First contiguous rainy window in the rest of today. */
        if (!same_date(time_iso, now_iso) || rain_closed) {
            continue;
        }
        if (slot.precip_prob_pct >= HG_WEATHER_RAIN_PROB_PCT) {
            if (next.rain_start_hour < 0) {
                next.rain_start_hour = (int8_t)hour;
            }
            next.rain_end_hour = (int8_t)hour;
        } else if (next.rain_start_hour >= 0) {
            rain_closed = true;
        }
    }
    if (next.hourly_count == 0) {
        HG_ERROR("weather", "Forecast has no hourly data\n");
        return false;
    }
    snapshot = next;
    return true;
}

static bool fetch_air(const hg_location_t *location) {
    char url[256];
    snprintf(url, sizeof(url),
             "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f"
             "&longitude=%.4f&current=us_aqi,pm2_5&timezone=auto",
             location->latitude, location->longitude);
    JsonDocument filter;
    filter["current"]["us_aqi"] = true;
    filter["current"]["pm2_5"] = true;
    JsonDocument doc;
    if (!hg_http_get_json(url, doc, filter)) {
        return false;
    }
    if (!doc["current"]["us_aqi"].is<float>()) {
        HG_ERROR("weather", "Air-quality response missing AQI\n");
        return false;
    }
    snapshot.us_aqi = (uint16_t)clamp_int((int)lroundf(doc["current"]["us_aqi"].as<float>()), 0, 999);
    snapshot.aqi_category = (uint8_t)hg_air_us_aqi_category(snapshot.us_aqi);
    snapshot.air_pm2_5 = doc["current"]["pm2_5"] | NAN;
    return true;
}

static hg_sample_status_t age_status(bool valid, bool failed, uint32_t ok_ms, uint32_t now) {
    if (!valid) {
        return failed ? HG_SAMPLE_ERROR : HG_SAMPLE_WAITING;
    }
    if (now - ok_ms >= HG_WEATHER_STALE_MS) {
        return HG_SAMPLE_STALE;
    }
    return failed ? HG_SAMPLE_ERROR : HG_SAMPLE_READY;
}

void hg_weather_init(void) {
    snapshot = {};
    snapshot.status = HG_SAMPLE_WAITING;
    snapshot.air_status = HG_SAMPLE_WAITING;
    snapshot.updated_minute_of_day = -1;
    snapshot.rain_start_hour = -1;
    snapshot.rain_end_hour = -1;
    snapshot.air_pm2_5 = NAN;
}

void hg_weather_update(void) {
    hg_location_t location;
    hg_location_get(&location);
    if (location.valid) {
        strlcpy(snapshot.location, location.name, sizeof(snapshot.location));
        /* A new location (fresh IP fix or override) invalidates data and refreshes now. */
        if (isnan(fetched_lat) || fabsf(location.latitude - fetched_lat) > 0.01f ||
            fabsf(location.longitude - fetched_lon) > 0.01f) {
            forecast_attempted = air_attempted = false;
            forecast_failed = air_failed = false;
            snapshot.valid = snapshot.air_valid = false;
            fetched_lat = location.latitude;
            fetched_lon = location.longitude;
        }
    }
    if (location.valid && WiFi.status() == WL_CONNECTED) {
        uint32_t now = millis();
        if (due(forecast_attempted, forecast_failed, forecast_ok_ms, forecast_attempt_ms,
                forecast_retry_ms, HG_WEATHER_REFRESH_MS, snapshot.valid, now)) {
            forecast_attempted = true;
            forecast_attempt_ms = now;
            forecast_failed = !fetch_forecast(&location);
            if (forecast_failed) {
                forecast_retry_ms = backoff(forecast_retry_ms);
            } else {
                forecast_retry_ms = HG_WEATHER_RETRY_MIN_MS;
                forecast_ok_ms = millis();
                snapshot.valid = true;
                snapshot.sampled_ms = forecast_ok_ms;
            }
        }
        now = millis();
        if (due(air_attempted, air_failed, air_ok_ms, air_attempt_ms, air_retry_ms,
                HG_AIR_REFRESH_MS, snapshot.air_valid, now)) {
            air_attempted = true;
            air_attempt_ms = now;
            air_failed = !fetch_air(&location);
            if (air_failed) {
                air_retry_ms = backoff(air_retry_ms);
            } else {
                air_retry_ms = HG_WEATHER_RETRY_MIN_MS;
                air_ok_ms = millis();
                snapshot.air_valid = true;
            }
        }
    }
    uint32_t now = millis();
    snapshot.status = age_status(snapshot.valid, forecast_failed, forecast_ok_ms, now);
    snapshot.air_status = age_status(snapshot.air_valid, air_failed, air_ok_ms, now);
}

void hg_weather_get_snapshot(hg_weather_snapshot_t *out) {
    if (out) {
        *out = snapshot;
    }
}
