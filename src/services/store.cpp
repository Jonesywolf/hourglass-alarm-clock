#include "hg/services/store.h"

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

#include "hg/config.h"
#include "hg/log.h"

struct alarm_record_t {
    uint32_t magic;
    uint8_t enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t volume_pct;
};

struct alarm_record_v2_t {
    alarm_record_t alarm;
    uint8_t sound;
    uint8_t reserved[3];
};

static_assert(sizeof(alarm_record_t) == 8, "Legacy alarm record layout changed");
static_assert(sizeof(alarm_record_v2_t) == 12, "Alarm v2 record layout changed");
static const uint32_t record_magic = 0x48474131;
static const uint32_t record_v2_magic = 0x48474132;

struct settings_record_t {
    uint32_t magic;
    uint8_t units;
    uint8_t reserved[3];
};

struct location_record_t {
    uint32_t magic;
    float latitude;
    float longitude;
    char name[HG_LOCATION_NAME_LEN];
};

static_assert(sizeof(settings_record_t) == 8, "Settings record layout changed");
static_assert(sizeof(location_record_t) == 12 + HG_LOCATION_NAME_LEN, "Location record layout changed");
static const uint32_t settings_magic = 0x48475331;
static const uint32_t location_magic = 0x48474C31;
static hg_settings_t desired_settings;
static hg_location_t desired_location;
static bool settings_seen = false;
static bool location_seen = false;
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
    out->sound = HG_ALARM_SOUND_MALLETS;
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

static bool decode_v2(const alarm_record_v2_t &record, hg_alarm_snapshot_t *out) {
    if (record.alarm.magic != record_v2_magic ||
        record.reserved[0] || record.reserved[1] || record.reserved[2]) {
        HG_ERROR("store", "Invalid saved alarm v2 record\n");
        return false;
    }
    alarm_record_t legacy = record.alarm;
    legacy.magic = record_magic;
    if (!decode_record(legacy, out)) {
        return false;
    }
    if (record.sound < HG_ALARM_SOUND_COUNT) {
        out->sound = (hg_alarm_sound_t)record.sound;
    } else {
        HG_ERROR("store", "Invalid saved alarm sound; using warm mallets\n");
    }
    return true;
}

#ifdef HG_RUNTIME_TEST
static bool fake_writer = false;
static bool fake_failure = false;
static uint32_t fake_delay_ms = 0;
static bool scratch_writer = false;

bool hg_store_test_decode_alarm(const uint8_t *bytes, size_t size, hg_alarm_snapshot_t *out) {
    if (size == sizeof(alarm_record_v2_t)) {
        alarm_record_v2_t record;
        memcpy(&record, bytes, sizeof(record));
        return decode_v2(record, out);
    }
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
    const char *key = "alarm_v2";
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
    const alarm_record_v2_t record = {
        {record_v2_magic, (uint8_t)in->enabled, (uint8_t)in->hour,
         (uint8_t)in->minute, in->volume_pct}, (uint8_t)in->sound, {0, 0, 0}};
    bool ok = prefs.putBytes(key, &record, sizeof(record)) == sizeof(record);
    prefs.end();
    return ok;
}

static bool fake_store_write(bool *ok) {
#ifdef HG_RUNTIME_TEST
    portENTER_CRITICAL(&mux);
    bool fake = fake_writer;
    bool failure = fake_failure;
    portEXIT_CRITICAL(&mux);
    if (fake) {
        *ok = !failure;
        return true;
    }
#endif
    (void)ok;
    return false;
}

static bool save_settings(const hg_settings_t *in) {
    bool ok = false;
    if (fake_store_write(&ok)) {
        return ok;
    }
    if (!prefs.begin("hg-store", false)) {
        return false;
    }
    const settings_record_t record = {settings_magic, (uint8_t)in->units, {0, 0, 0}};
    ok = prefs.putBytes("settings_v1", &record, sizeof(record)) == sizeof(record);
    prefs.end();
    return ok;
}

static bool save_location(const hg_location_t *in) {
    bool ok = false;
    if (fake_store_write(&ok)) {
        return ok;
    }
    if (!prefs.begin("hg-store", false)) {
        return false;
    }
    location_record_t record = {};
    record.magic = location_magic;
    record.latitude = in->latitude;
    record.longitude = in->longitude;
    memcpy(record.name, in->name, sizeof(record.name));
    record.name[sizeof(record.name) - 1] = '\0';
    ok = prefs.putBytes("location_v1", &record, sizeof(record)) == sizeof(record);
    prefs.end();
    return ok;
}

static void store_worker(void *) {
    bool failed = false;
    uint32_t last_failure_ms = 0;
    for (;;) {
        hg_alarm_snapshot_t value;
        hg_settings_t settings_value;
        hg_location_t location_value;
        uint32_t generation = 0;
        bool write_settings = false;
        bool write_location = false;
        portENTER_CRITICAL(&mux);
        bool admitted = flash_allowed &&
                        (!failed || millis() - last_failure_ms >= HG_STORE_RETRY_MS);
        if (admitted && status.pending) {
            value = desired;
            generation = status.requested_generation;
            status.writing = true;
        } else if (admitted && status.settings_pending) {
            settings_value = desired_settings;
            status.settings_pending = false;
            write_settings = true;
            status.writing = true;
        } else if (admitted && status.location_pending) {
            location_value = desired_location;
            status.location_pending = false;
            write_location = true;
            status.writing = true;
        }
        portEXIT_CRITICAL(&mux);
        if (write_settings || write_location) {
            uint32_t started_us = micros();
            bool ok = write_settings ? save_settings(&settings_value) :
                                       save_location(&location_value);
            uint32_t elapsed_us = micros() - started_us;
            portENTER_CRITICAL(&mux);
            status.writing = false;
            status.last_write_us = elapsed_us;
            if (ok) {
                ++status.successful_saves;
            } else {
                ++status.failures;
                /* Re-arm unless a newer request already did. */
                if (write_settings) {
                    status.settings_pending = true;
                } else {
                    status.location_pending = true;
                }
            }
            portEXIT_CRITICAL(&mux);
            failed = !ok;
            if (!ok) {
                last_failure_ms = millis();
                HG_ERROR("store", "%s save failed; retry pending\n",
                         write_settings ? "Settings" : "Location");
            }
        }
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
    desired_settings = {};
    desired_location = {};
    status = {};
    flash_allowed = false;
    request_seen = false;
    settings_seen = false;
    location_seen = false;
}

bool hg_store_load_settings(hg_settings_t *out) {
    if (!out) {
        return false;
    }
    out->units = HG_DEFAULT_UNITS;
    if (!prefs.begin("hg-store", true)) {
        return false;
    }
    settings_record_t record = {};
    bool found = prefs.isKey("settings_v1") &&
                 prefs.getBytesLength("settings_v1") == sizeof(record) &&
                 prefs.getBytes("settings_v1", &record, sizeof(record)) == sizeof(record);
    prefs.end();
    if (!found) {
        return false;
    }
    if (record.magic != settings_magic || record.units >= HG_UNITS_COUNT) {
        HG_ERROR("store", "Invalid saved settings; using defaults\n");
        return false;
    }
    out->units = (hg_unit_system_t)record.units;
    portENTER_CRITICAL(&mux);
    desired_settings = *out;
    settings_seen = true;
    portEXIT_CRITICAL(&mux);
    return true;
}

void hg_store_request_save_settings(const hg_settings_t *in) {
    if (!in || (unsigned)in->units >= HG_UNITS_COUNT) {
        HG_ERROR("store", "Invalid settings save request\n");
        return;
    }
    portENTER_CRITICAL(&mux);
    if (!settings_seen || desired_settings.units != in->units) {
        desired_settings = *in;
        settings_seen = true;
        status.settings_pending = true;
    }
    portEXIT_CRITICAL(&mux);
}

bool hg_store_load_location(hg_location_t *out) {
    if (!out) {
        return false;
    }
    *out = {};
    if (!prefs.begin("hg-store", true)) {
        return false;
    }
    location_record_t record = {};
    bool found = prefs.isKey("location_v1") &&
                 prefs.getBytesLength("location_v1") == sizeof(record) &&
                 prefs.getBytes("location_v1", &record, sizeof(record)) == sizeof(record);
    prefs.end();
    if (!found || record.magic != location_magic || !(record.latitude >= -90.0f) ||
        !(record.latitude <= 90.0f) || !(record.longitude >= -180.0f) ||
        !(record.longitude <= 180.0f)) {
        return false;
    }
    record.name[sizeof(record.name) - 1] = '\0';
    out->valid = true;
    out->latitude = record.latitude;
    out->longitude = record.longitude;
    memcpy(out->name, record.name, sizeof(out->name));
    out->source = HG_LOCATION_CACHED;
    portENTER_CRITICAL(&mux);
    desired_location = *out;
    location_seen = true;
    portEXIT_CRITICAL(&mux);
    return true;
}

void hg_store_request_save_location(const hg_location_t *in) {
    if (!in || !in->valid) {
        return;
    }
    portENTER_CRITICAL(&mux);
    /* ~1 km tolerance avoids rewriting flash for IP-geolocation jitter. */
    bool changed = !location_seen ||
                   fabsf(desired_location.latitude - in->latitude) > 0.01f ||
                   fabsf(desired_location.longitude - in->longitude) > 0.01f ||
                   strncmp(desired_location.name, in->name, sizeof(in->name)) != 0;
    if (changed) {
        desired_location = *in;
        location_seen = true;
        status.location_pending = true;
    }
    portEXIT_CRITICAL(&mux);
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
    if (prefs.isKey("alarm_v2")) {
        alarm_record_v2_t record = {};
        found = prefs.getBytesLength("alarm_v2") == sizeof(record) &&
                prefs.getBytes("alarm_v2", &record, sizeof(record)) == sizeof(record) &&
                decode_v2(record, out);
        if (!found) {
            HG_ERROR("store", "Invalid saved alarm v2 record; not loading older settings\n");
        }
    } else if (prefs.isKey("alarm_v1")) {
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
            out->sound = HG_ALARM_SOUND_MALLETS;
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
        in->volume_pct % HG_ALARM_VOLUME_STEP_PCT != 0 ||
        (unsigned)in->sound >= HG_ALARM_SOUND_COUNT) {
        HG_ERROR("store", "Invalid alarm save request\n");
        return;
    }
    portENTER_CRITICAL(&mux);
    if (!request_seen || desired.enabled != in->enabled ||
        desired.hour != in->hour || desired.minute != in->minute ||
        desired.volume_pct != in->volume_pct || desired.sound != in->sound) {
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
