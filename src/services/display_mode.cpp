#include "hg/services/display_mode.h"

#include <Arduino.h>

#include "hg/config.h"
#include "hg/services/alarm.h"
#include "hg/services/brightness.h"
#include "hg/services/time.h"
#include "hg/types.h"

static hg_display_mode_t mode = HG_DISPLAY_MODE_NORMAL;
static uint32_t entered_red_ms = 0;

static bool hg_display_mode_pre_alarm_window(void) {
    hg_alarm_snapshot_t alarm;
    hg_time_snapshot_t time;
    hg_alarm_get_snapshot(&alarm);
    hg_time_get_snapshot(&time);

    if (!alarm.enabled || !time.valid) {
        return false;
    }

    int now_min = time.hour_24 * 60 + time.minute;
    int alarm_min = alarm.hour * 60 + alarm.minute;
    int diff = alarm_min - now_min;
    if (diff < 0) {
        diff += 24 * 60;
    }
    return diff <= HG_DISPLAY_PRE_ALARM_MIN;
}

static void hg_display_mode_enter(hg_display_mode_t next) {
    if (next == HG_DISPLAY_MODE_RED && mode != HG_DISPLAY_MODE_RED) {
        entered_red_ms = millis();
    }
    mode = next;
}

void hg_display_mode_init(void) {
    mode = HG_DISPLAY_MODE_NORMAL;
    entered_red_ms = 0;
}

void hg_display_mode_update(void) {
    hg_light_level_t level = hg_brightness_get_light_level();

    if (level == HG_LIGHT_BRIGHT) {
        hg_display_mode_enter(HG_DISPLAY_MODE_NORMAL);
        return;
    }

    switch (mode) {
        case HG_DISPLAY_MODE_NORMAL:
            if (level == HG_LIGHT_DARK) {
                hg_display_mode_enter(HG_DISPLAY_MODE_RED);
            }
            break;

        case HG_DISPLAY_MODE_RED:
            if (millis() - entered_red_ms >= (uint32_t)HG_DISPLAY_RED_TO_OFF_MIN * 60000UL) {
                hg_display_mode_enter(HG_DISPLAY_MODE_OFF);
            }
            break;

        case HG_DISPLAY_MODE_OFF: {
            hg_alarm_snapshot_t alarm;
            hg_alarm_get_snapshot(&alarm);
            if (alarm.ringing || hg_display_mode_pre_alarm_window()) {
                hg_display_mode_enter(HG_DISPLAY_MODE_RED);
            }
            break;
        }
    }
}

hg_display_mode_t hg_display_mode_get(void) {
    return mode;
}
