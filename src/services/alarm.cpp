#include "hg/services/alarm.h"

#include <Arduino.h>

#include "hg/config.h"
#include "hg/hal/speaker.h"
#include "hg/log.h"
#include "hg/services/store.h"
#include "hg/services/time.h"

static hg_alarm_snapshot_t snapshot;
static bool dirty = false;  /* true when snapshot has unsaved changes pending store write */
static uint32_t last_change_ms = 0;
static bool fired_this_window = false;

static void hg_alarm_stop_ringing(void) {
    if (!snapshot.ringing) {
        return;
    }
    snapshot.ringing = false;
    hg_speaker_set_mode(HG_SPEAKER_STOP);
}

void hg_alarm_init(void) {
    snapshot.enabled = false;
    snapshot.ringing = false;
    snapshot.hour = 7;
    snapshot.minute = 0;
    hg_store_load_alarm(&snapshot);
    snapshot.ringing = false;
    dirty = false;
    fired_this_window = false;
}

void hg_alarm_update(void) {
    if (dirty && (millis() - last_change_ms >= HG_ALARM_STORE_DEBOUNCE_MS)) {
        hg_store_save_alarm(&snapshot);
        dirty = false;
    }

    hg_time_snapshot_t time;
    hg_time_get_snapshot(&time);

    if (!snapshot.enabled || !time.valid) {
        fired_this_window = false;
        return;
    }

    bool in_window = (time.hour_24 == snapshot.hour && time.minute == snapshot.minute);
    if (!in_window) {
        fired_this_window = false;
        return;
    }

    if (fired_this_window || snapshot.ringing) {
        return;
    }

    snapshot.ringing = true;
    fired_this_window = true;
    hg_speaker_set_mode(HG_SPEAKER_HARD);
    HG_LOG("alarm", "ringing %02d:%02d\n", snapshot.hour, snapshot.minute);
}

void hg_alarm_get_snapshot(hg_alarm_snapshot_t *out) {
    if (out) {
        *out = snapshot;
    }
}

void hg_alarm_set_time(int hour, int minute) {
    snapshot.hour = hour;
    snapshot.minute = minute;
    dirty = true;
    last_change_ms = millis();
}

void hg_alarm_set_enabled(bool enabled) {
    snapshot.enabled = enabled;
    if (!enabled) {
        hg_alarm_stop_ringing();
        fired_this_window = false;
    }
    hg_store_save_alarm(&snapshot);
}

void hg_alarm_dismiss(void) {
    HG_LOG("alarm", "dismiss\n");
    hg_alarm_stop_ringing();
    fired_this_window = true;
}
