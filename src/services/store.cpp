#include "hg/services/store.h"

#include <Preferences.h>

static Preferences prefs;

static const char *const HG_STORE_NAMESPACE = "hg-store";
static const char *const HG_STORE_KEY_ALARM_ENABLED = "alarm_en";
static const char *const HG_STORE_KEY_ALARM_HOUR = "alarm_hr";
static const char *const HG_STORE_KEY_ALARM_MINUTE = "alarm_min";

void hg_store_init(void) {}

bool hg_store_load_alarm(hg_alarm_snapshot_t *out) {
    if (!out) {
        return false;
    }

    if (!prefs.begin(HG_STORE_NAMESPACE, true)) {
        return false;
    }

    bool has_value = prefs.isKey(HG_STORE_KEY_ALARM_HOUR);
    if (has_value) {
        out->enabled = prefs.getBool(HG_STORE_KEY_ALARM_ENABLED, out->enabled);
        out->hour = prefs.getInt(HG_STORE_KEY_ALARM_HOUR, out->hour);
        out->minute = prefs.getInt(HG_STORE_KEY_ALARM_MINUTE, out->minute);
    }

    prefs.end();
    return has_value;
}

bool hg_store_save_alarm(const hg_alarm_snapshot_t *in) {
    if (!in) {
        return false;
    }

    if (!prefs.begin(HG_STORE_NAMESPACE, false)) {
        return false;
    }

    prefs.putBool(HG_STORE_KEY_ALARM_ENABLED, in->enabled);
    prefs.putInt(HG_STORE_KEY_ALARM_HOUR, in->hour);
    prefs.putInt(HG_STORE_KEY_ALARM_MINUTE, in->minute);

    prefs.end();
    return true;
}
