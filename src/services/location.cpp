#include "hg/services/location.h"

#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

#include "hg/config.h"
#include "hg/log.h"
#include "hg/services/store.h"
#include "http_internal.h"
#include "secrets_internal.h"

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static hg_location_t current;
static bool override_set = false;
static bool fetched = false;
static uint32_t last_success_ms = 0;
static uint32_t last_attempt_ms = 0;
static uint32_t retry_ms = HG_WEATHER_RETRY_MIN_MS;
static bool attempted = false;

static void set_current(const hg_location_t *value) {
    portENTER_CRITICAL(&mux);
    current = *value;
    portEXIT_CRITICAL(&mux);
}

void hg_location_seed(const hg_location_t *cached) {
    if (cached && cached->valid) {
        set_current(cached);
    }
}

void hg_location_init(void) {
    float lat = 0.0f;
    float lon = 0.0f;
    const char *name = "";
    override_set = hg_secret_location(&lat, &lon, &name);
    if (override_set) {
        hg_location_t value = {};
        value.valid = true;
        value.latitude = lat;
        value.longitude = lon;
        strlcpy(value.name, name, sizeof(value.name));
        value.source = HG_LOCATION_OVERRIDE;
        set_current(&value);
    }
}

static bool fetch_ip_location(hg_location_t *out) {
    JsonDocument filter;
    filter["latitude"] = true;
    filter["longitude"] = true;
    filter["city"] = true;
    filter["error"] = true;
    JsonDocument doc;
    if (!hg_http_get_json("https://ipapi.co/json/", doc, filter)) {
        return false;
    }
    if (doc["error"].as<bool>() || !doc["latitude"].is<float>() || !doc["longitude"].is<float>()) {
        HG_ERROR("location", "IP geolocation response rejected\n");
        return false;
    }
    float lat = doc["latitude"].as<float>();
    float lon = doc["longitude"].as<float>();
    if (!(lat >= -90.0f && lat <= 90.0f && lon >= -180.0f && lon <= 180.0f)) {
        return false;
    }
    *out = {};
    out->valid = true;
    out->latitude = lat;
    out->longitude = lon;
    strlcpy(out->name, doc["city"] | "", sizeof(out->name));
    out->source = HG_LOCATION_IP;
    return true;
}

void hg_location_update(void) {
    if (override_set || WiFi.status() != WL_CONNECTED) {
        return;
    }
    uint32_t now = millis();
    if (fetched && now - last_success_ms < HG_LOCATION_REFRESH_MS) {
        return;
    }
    if (attempted && now - last_attempt_ms < retry_ms) {
        return;
    }
    attempted = true;
    last_attempt_ms = now;
    hg_location_t value;
    if (!fetch_ip_location(&value)) {
        retry_ms = retry_ms >= HG_WEATHER_RETRY_MAX_MS / 2 ? HG_WEATHER_RETRY_MAX_MS : retry_ms * 2;
        return;
    }
    retry_ms = HG_WEATHER_RETRY_MIN_MS;
    fetched = true;
    last_success_ms = now;
    set_current(&value);
    hg_store_request_save_location(&value);
}

void hg_location_get(hg_location_t *out) {
    if (out) {
        portENTER_CRITICAL(&mux);
        *out = current;
        portEXIT_CRITICAL(&mux);
    }
}
