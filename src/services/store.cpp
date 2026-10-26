#include "hg/services/store.h"

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "hg/config.h"
#include "hg/log.h"

struct alarm_record_t {
    uint32_t magic;
    uint8_t enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t volume_pct;
};

static const uint32_t record_magic = 0x48474131;
static Preferences prefs;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static hg_alarm_snapshot_t desired;
static hg_store_status_t status;
static bool flash_allowed = false;
static bool request_seen = false;
static TaskHandle_t worker = nullptr;

static bool decode_record(const alarm_record_t &record, hg_alarm_snapshot_t *out) {
    if (record.magic != record_magic || record.enabled > 1 ||
        record.hour >= 24 || record.minute >= 60) {
        HG_ERROR("store", "Invalid saved alarm record\n");
        return false;
    }
    out->enabled = record.enabled != 0;
    out->hour = record.hour;
    out->minute = record.minute;
    out->volume_pct = HG_ALARM_VOLUME_DEFAULT_PCT;
    if (record.volume_pct != 0) {
        if (record.volume_pct >= HG_ALARM_VOLUME_MIN_PCT && record.volume_pct <= 100 &&
            record.volume_pct % HG_ALARM_VOLUME_STEP_PCT == 0) {
            out->volume_pct = record.volume_pct;
        } else {
            HG_ERROR("store", "Invalid saved alarm volume; using default\n");
        }
    }
    return true;
}

#ifdef HG_RUNTIME_TEST
static bool fake_writer = false;
static bool fake_failure = false;
static uint32_t fake_delay_ms = 0;
static bool scratch_writer = false;

bool hg_store_test_decode_alarm(const uint8_t *bytes, size_t size, hg_alarm_snapshot_t *out) {
    if (size != sizeof(alarm_record_t)) {
        HG_ERROR("store", "Invalid saved alarm record size\n");
        return false;
    }
    alarm_record_t record;
    memcpy(&record, bytes, sizeof(record));
    return decode_record(record, out);
}

void hg_store_test_get_desired(hg_alarm_snapshot_t *out) {
    portENTER_CRITICAL(&mux);
    *out = desired;
    portEXIT_CRITICAL(&mux);
}

void hg_store_test_writer(uint32_t delay_ms, bool fail) {
    portENTER_CRITICAL(&mux);
    fake_writer = true;
    fake_delay_ms = delay_ms;
    fake_failure = fail;
    scratch_writer = false;
    portEXIT_CRITICAL(&mux);
}

void hg_store_test_scratch(bool enabled) {
    portENTER_CRITICAL(&mux);
    fake_writer = true;
    scratch_writer = enabled;
    fake_failure = false;
    fake_delay_ms = 0;
    portEXIT_CRITICAL(&mux);
}
#endif

static bool save_alarm(const hg_alarm_snapshot_t *in) {
    const char *name = "hg-store";
    const char *key = "alarm_v1";
#ifdef HG_RUNTIME_TEST
    portENTER_CRITICAL(&mux);
    bool fake = fake_writer;
    bool failure = fake_failure;
    uint32_t wait_ms = fake_delay_ms;
    bool scratch = scratch_writer;
    portEXIT_CRITICAL(&mux);
    if (fake) {
        delay(wait_ms);
        if (!scratch || failure) {
            return !failure;
        }
        name = "hg-rt-test";
        key = "probe";
    }
#endif
    if (!prefs.begin(name, false)) {
        return false;
    }
    const alarm_record_t record = {record_magic, (uint8_t)in->enabled,
                                   (uint8_t)in->hour, (uint8_t)in->minute, in->volume_pct};
    bool ok = prefs.putBytes(key, &record, sizeof(record)) == sizeof(record);
    prefs.end();
    return ok;
}

static void store_worker(void *) {
    bool failed = false;
    uint32_t last_failure_ms = 0;
    for (;;) {
        hg_alarm_snapshot_t value;
        uint32_t generation = 0;
        portENTER_CRITICAL(&mux);
        if (status.pending && flash_allowed &&
            (!failed || millis() - last_failure_ms >= HG_STORE_RETRY_MS)) {
            value = desired;
            generation = status.requested_generation;
            status.writing = true;
        }
        portEXIT_CRITICAL(&mux);
        if (generation != 0) {
            uint32_t started_us = micros();
            bool ok = save_alarm(&value);
            uint32_t elapsed_us = micros() - started_us;
            portENTER_CRITICAL(&mux);
            status.writing = false;
            status.last_write_us = elapsed_us;
            if (ok) {
                ++status.successful_saves;
                status.saved_generation = generation;
                status.pending = status.requested_generation != generation;
            } else {
                ++status.failures;
            }
            portEXIT_CRITICAL(&mux);
            failed = !ok;
            if (!ok) {
                last_failure_ms = millis();
                HG_ERROR("store", "Alarm save failed; retry pending\n");
            }
        }
        uint32_t margin = uxTaskGetStackHighWaterMark(nullptr);
        portENTER_CRITICAL(&mux);
        status.stack_free_bytes = margin;
        portEXIT_CRITICAL(&mux);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void hg_store_init(void) {
    configASSERT(!worker);
    desired = {};
    status = {};
    flash_allowed = false;
    request_seen = false;
}

bool hg_store_start_worker(void) {
    return xTaskCreatePinnedToCore(store_worker, "hg-store", 3072, nullptr,
                                  1, &worker, 0) == pdPASS;
}

bool hg_store_load_alarm(hg_alarm_snapshot_t *out) {
    if (!out) {
        HG_ERROR("store", "Missing alarm load destination\n");
        return false;
    }
    if (!prefs.begin("hg-store", true)) {
        HG_LOG("store", "No saved alarm namespace\n");
        return false;
    }
    bool found = false;
    if (prefs.isKey("alarm_v1")) {
        alarm_record_t record = {};
        found = prefs.getBytesLength("alarm_v1") == sizeof(record) &&
                prefs.getBytes("alarm_v1", &record, sizeof(record)) == sizeof(record) &&
                decode_record(record, out);
        if (!found) {
            HG_ERROR("store", "Invalid saved alarm record\n");
        }
    } else if (prefs.isKey("alarm_hr")) {
        int hour = prefs.getInt("alarm_hr", out->hour);
        int minute = prefs.getInt("alarm_min", out->minute);
        found = hour >= 0 && hour < 24 && minute >= 0 && minute < 60;
        if (found) {
            out->enabled = prefs.getBool("alarm_en", out->enabled);
            out->hour = hour;
            out->minute = minute;
            out->volume_pct = HG_ALARM_VOLUME_DEFAULT_PCT;
        } else {
            HG_ERROR("store", "Invalid legacy alarm settings\n");
        }
    }
    prefs.end();
    return found;
}

void hg_store_request_save_alarm(const hg_alarm_snapshot_t *in) {
    if (!in || in->hour < 0 || in->hour >= 24 || in->minute < 0 || in->minute >= 60 ||
        in->volume_pct < HG_ALARM_VOLUME_MIN_PCT || in->volume_pct > 100 ||
        in->volume_pct % HG_ALARM_VOLUME_STEP_PCT != 0) {
        HG_ERROR("store", "Invalid alarm save request\n");
        return;
    }
    portENTER_CRITICAL(&mux);
    if (!request_seen || desired.enabled != in->enabled ||
        desired.hour != in->hour || desired.minute != in->minute ||
        desired.volume_pct != in->volume_pct) {
        desired = *in;
        request_seen = true;
        ++status.requested_generation;
        if (status.requested_generation == 0) {
            ++status.requested_generation;
        }
        status.pending = true;
    }
    portEXIT_CRITICAL(&mux);
}

void hg_store_set_flash_allowed(bool allowed) {
    portENTER_CRITICAL(&mux);
    flash_allowed = allowed;
    portEXIT_CRITICAL(&mux);
}

void hg_store_get_status(hg_store_status_t *out) {
    if (out) {
        portENTER_CRITICAL(&mux);
        *out = status;
        portEXIT_CRITICAL(&mux);
    }
}
