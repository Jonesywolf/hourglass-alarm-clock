#include "hg/services/alarm.h"

#include <Arduino.h>

#include "hg/config.h"
#include "hg/services/store.h"

static hg_alarm_snapshot_t snapshot;
static bool dirty = false;  /* true when snapshot has unsaved changes pending store write */
static uint32_t last_change_ms = 0;

void hg_alarm_init(void) {
    snapshot.enabled = false;
    snapshot.hour = 7;
    snapshot.minute = 0;
    hg_store_load_alarm(&snapshot);
    dirty = false;
}

void hg_alarm_update(void) {
    if (!dirty) {
        return;
    }

    if (millis() - last_change_ms >= HG_ALARM_STORE_DEBOUNCE_MS) {
        hg_store_save_alarm(&snapshot);
        dirty = false;
    }
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
    hg_store_save_alarm(&snapshot);
}
