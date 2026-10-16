#include "hg/services/display_mode.h"

#include <Arduino.h>

#include "hg/config.h"
#include "hg/services/alarm.h"
#include "hg/services/brightness.h"
#include "hg/services/time.h"
#include "hg/types.h"

static hg_display_mode_t mode = HG_DISPLAY_MODE_NORMAL;
static uint32_t entered_red_ms = 0;
static bool input_wake = false;
static uint32_t last_input_ms = 0;

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

static void hg_display_mode_enter(hg_display_mode_t next, uint32_t now) {
    if (next == HG_DISPLAY_MODE_RED && mode != HG_DISPLAY_MODE_RED) {
        entered_red_ms = now;
    }
    mode = next;
}

void hg_display_mode_init(void) {
    mode = HG_DISPLAY_MODE_NORMAL;
    entered_red_ms = 0;
    input_wake = false;
    last_input_ms = 0;
}

void hg_display_mode_update(void) {
    hg_display_mode_update(millis());
}

bool hg_display_mode_note_activity(uint32_t now_ms) {
    bool woke = mode == HG_DISPLAY_MODE_OFF;
    if (woke) {
        hg_display_mode_enter(HG_DISPLAY_MODE_RED, now_ms);
        input_wake = true;
    }
    if (input_wake) {
        last_input_ms = now_ms;
    }
    return woke;
}

void hg_display_mode_update(uint32_t now_ms) {
    hg_light_level_t level = hg_brightness_get_light_level();

    if (level == HG_LIGHT_BRIGHT) {
        input_wake = false;
        hg_display_mode_enter(HG_DISPLAY_MODE_NORMAL, now_ms);
        return;
    }

    hg_alarm_snapshot_t alarm;
    hg_alarm_get_snapshot(&alarm);
    bool alarm_visible = alarm.ringing || alarm.snoozed || hg_display_mode_pre_alarm_window();
    if (alarm_visible && mode == HG_DISPLAY_MODE_OFF) {
        hg_display_mode_enter(HG_DISPLAY_MODE_RED, now_ms);
    }

    switch (mode) {
        case HG_DISPLAY_MODE_NORMAL:
            if (level == HG_LIGHT_DARK) {
                hg_display_mode_enter(HG_DISPLAY_MODE_RED, now_ms);
            }
            break;

        case HG_DISPLAY_MODE_RED:
            if (!alarm_visible &&
                (input_wake ? now_ms - last_input_ms >= HG_DISPLAY_INPUT_WAKE_MS :
                 now_ms - entered_red_ms >= (uint32_t)HG_DISPLAY_RED_TO_OFF_MIN * 60000UL)) {
                hg_display_mode_enter(HG_DISPLAY_MODE_OFF, now_ms);
            }
            break;

        case HG_DISPLAY_MODE_OFF:
            break;
    }
}

hg_display_mode_t hg_display_mode_get(void) {
    return mode;
}
