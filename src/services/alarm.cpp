#include "hg/services/alarm.h"

#include <Arduino.h>

#include "hg/config.h"
#include "hg/log.h"
#include "hg/services/store.h"
#include "hg/services/time.h"

/* snapshot.hour/minute is the effective ring time (the snoozed time while snoozed);
 * base_* is the saved time. Only base_* is ever persisted. */
static hg_alarm_snapshot_t snapshot;
static int base_hour = 7;
static int base_minute = 0;
static bool dirty = false;  /* true when snapshot has unsaved changes pending store write */
static uint32_t last_change_ms = 0;
static bool fired_this_window = false;
static bool acted = false;
static uint32_t acted_ms = 0;

static void hg_alarm_request_save(void) {
    hg_alarm_snapshot_t saved = {snapshot.enabled, false, base_hour, base_minute, false};
    hg_store_request_save_alarm(&saved);
}

static void hg_alarm_restore_base(void) {
    snapshot.hour = base_hour;
    snapshot.minute = base_minute;
    snapshot.snoozed = false;
}

static void hg_alarm_note_action(void) {
    acted = true;
    acted_ms = millis();
}

static void hg_alarm_stop_ringing(void) {
    if (!snapshot.ringing) {
        return;
    }
    snapshot.ringing = false;
}

void hg_alarm_init(void) {
    snapshot = {};
    snapshot.hour = 7;
    hg_store_load_alarm(&snapshot);
    snapshot.ringing = false;
    snapshot.snoozed = false;
    base_hour = snapshot.hour;
    base_minute = snapshot.minute;
    dirty = false;
    fired_this_window = false;
    acted = false;
}

void hg_alarm_update(void) {
    if (dirty && (millis() - last_change_ms >= HG_ALARM_STORE_DEBOUNCE_MS)) {
        hg_alarm_request_save();
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
    HG_LOG("alarm", "ringing %02d:%02d\n", snapshot.hour, snapshot.minute);
}

void hg_alarm_get_snapshot(hg_alarm_snapshot_t *out) {
    if (out) {
        *out = snapshot;
    }
}

void hg_alarm_set_time(int hour, int minute) {
    base_hour = hour;
    base_minute = minute;
    hg_alarm_restore_base();
    dirty = true;
    last_change_ms = millis();
}

void hg_alarm_set_enabled(bool enabled) {
    snapshot.enabled = enabled;
    if (!enabled) {
        hg_alarm_stop_ringing();
        hg_alarm_restore_base();
        fired_this_window = false;
    }
    hg_alarm_request_save();
}

void hg_alarm_dismiss(void) {
    HG_LOG("alarm", "dismiss\n");
    hg_alarm_stop_ringing();
    hg_alarm_restore_base();
    fired_this_window = true;
    hg_alarm_note_action();
}

void hg_alarm_snooze(void) {
    if (!snapshot.ringing) {
        return;
    }
    hg_time_snapshot_t time;
    hg_time_get_snapshot(&time);
    int minutes = time.valid ? time.hour_24 * 60 + time.minute
                             : snapshot.hour * 60 + snapshot.minute;
    minutes = (minutes + HG_ALARM_SNOOZE_MIN) % (24 * 60);
    snapshot.hour = minutes / 60;
    snapshot.minute = minutes % 60;
    snapshot.snoozed = true;
    snapshot.ringing = false;
    fired_this_window = true;
    hg_alarm_note_action();
    HG_LOG("alarm", "snooze until %02d:%02d\n", snapshot.hour, snapshot.minute);
}

bool hg_alarm_toggle_allowed(uint32_t now_ms) {
    return !acted || now_ms - acted_ms >= HG_ALARM_TOGGLE_LOCKOUT_MS;
}
