#include "hg/services/alarm.h"

#include "hg/services/store.h"

static hg_alarm_snapshot_t snapshot;

void hg_alarm_init(void) {
    snapshot.enabled = false;
    snapshot.hour = 7;
    snapshot.minute = 0;
    hg_store_load_alarm(&snapshot);
}

void hg_alarm_update(void) {}

void hg_alarm_get_snapshot(hg_alarm_snapshot_t *out) {
    if (out) {
        *out = snapshot;
    }
}

void hg_alarm_set_time(int hour, int minute) {
    snapshot.hour = hour;
    snapshot.minute = minute;
    hg_store_save_alarm(&snapshot);
}

void hg_alarm_set_enabled(bool enabled) {
    snapshot.enabled = enabled;
    hg_store_save_alarm(&snapshot);
}
