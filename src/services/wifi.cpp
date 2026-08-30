#include "hg/services/wifi.h"

#include <Arduino.h>
#include <WiFi.h>

#include "hg/config.h"
#include "secrets.h"

static uint32_t last_retry_ms = 0;
static uint32_t retry_interval_ms = HG_WIFI_RETRY_INITIAL_MS;
static uint32_t last_begin_ms = 0;

static hg_wifi_bars_t bars_from_rssi(int rssi) {
    if (rssi >= -55) {
        return HG_WIFI_BARS_4;
    }
    if (rssi >= -65) {
        return HG_WIFI_BARS_3;
    }
    if (rssi >= -75) {
        return HG_WIFI_BARS_2;
    }
    if (rssi >= -85) {
        return HG_WIFI_BARS_1;
    }
    return HG_WIFI_BARS_NONE;
}

void hg_wifi_init(void) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    retry_interval_ms = HG_WIFI_RETRY_INITIAL_MS;
    last_retry_ms = 0;
    last_begin_ms = millis();
    Serial.printf("Connecting to %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void hg_wifi_update(void) {
    wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        retry_interval_ms = HG_WIFI_RETRY_INITIAL_MS;
        last_retry_ms = 0;
        last_begin_ms = 0;
        return;
    }

    uint32_t now = millis();

    if (last_begin_ms != 0 && (now - last_begin_ms) < HG_WIFI_CONNECT_ATTEMPT_MS) {
        return;
    }

    if (last_retry_ms != 0 && (now - last_retry_ms) < retry_interval_ms) {
        return;
    }

    last_retry_ms = now;
    last_begin_ms = now;
    Serial.printf("Retrying Wi-Fi connection (backoff %lu ms)...\n", retry_interval_ms);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t next_interval = retry_interval_ms << 1U;
    if (next_interval < retry_interval_ms || next_interval > HG_WIFI_RETRY_MAX_MS) {
        retry_interval_ms = HG_WIFI_RETRY_MAX_MS;
    } else {
        retry_interval_ms = next_interval;
    }
}

void hg_wifi_get_snapshot(hg_wifi_snapshot_t *out) {
    if (!out) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        out->state = HG_WIFI_CONNECTED;
        out->rssi = WiFi.RSSI();
        out->bars = bars_from_rssi(out->rssi);
        return;
    }

    out->state = HG_WIFI_CONNECTING;
    out->rssi = 0;
    out->bars = HG_WIFI_BARS_NONE;
}

const char *hg_wifi_get_target_ssid(void) {
    return WIFI_SSID;
}
