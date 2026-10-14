#include "hg/app/input_router.h"

#include <Arduino.h>

#include "hg/app/runtime.h"
#include "hg/config.h"
#include "hg/log.h"
#include "hg/services/alarm.h"
#include "hg/services/effects.h"
#include "hg/types.h"
#include "hg/ui/clock_screen.h"
#include "hg/ui/navigator.h"

static int32_t pending_delta = 0;
static bool pending_toggle = false;

static void hg_input_router_apply_encoder(int32_t delta) {
    if (delta == 0) {
        return;
    }

    hg_alarm_snapshot_t alarm;
    hg_alarm_get_snapshot(&alarm);
    if (!alarm.enabled) {
        return;
    }

    int total_minutes = alarm.hour * 60 + alarm.minute + (int)(delta % (24 * 60));
    total_minutes %= (24 * 60);
    if (total_minutes < 0) {
        total_minutes += 24 * 60;
    }

    hg_alarm_set_time(total_minutes / 60, total_minutes % 60);

    /* Refresh immediately instead of waiting for the next periodic UI tick. */
    hg_alarm_get_snapshot(&alarm);
    hg_clock_screen_update_alarm(&alarm);
    hg_effects_request_feedback(HG_FEEDBACK_ALARM_EDIT, millis());
}

void hg_input_router_init(void) {
    pending_delta = 0;
    pending_toggle = false;
}

bool hg_input_router_update(bool navigation_allowed) {
    bool activity = false;
    if (!navigation_allowed) {
        pending_delta = 0;
        pending_toggle = false;
    }
    uint32_t generation = hg_runtime_input_generation();
    for (unsigned i = 0; i < HG_INPUT_QUEUE_CAPACITY; ++i) {
        hg_input_event_t event;
        if (!hg_runtime_take_input(&event)) {
            break;
        }
        activity = activity || event.tapped || event.encoder_pressed || event.encoder_delta != 0;
        bool allowed = navigation_allowed && event.generation == generation;
        if (event.tapped) {
            hg_alarm_snapshot_t alarm;
            hg_alarm_get_snapshot(&alarm);
            HG_TOUCH_LOG("input", "tap action=%s page=%d nav=%d gen=%lu/%lu age=%lu ms\n",
                         alarm.ringing ? "dismiss" : allowed ? "next" : "blocked",
                         (int)hg_navigator_current(), navigation_allowed ? 1 : 0,
                         (unsigned long)event.generation, (unsigned long)generation,
                         (unsigned long)(millis() - event.captured_ms));
            if (alarm.ringing) {
                hg_alarm_dismiss();
                hg_effects_set_alarm(false, false);
            } else if (allowed) {
                hg_navigator_next();
                hg_effects_request_feedback(HG_FEEDBACK_NAVIGATE, millis());
            }
        }
        if (allowed) {
            pending_toggle = pending_toggle != event.encoder_pressed;
            pending_delta = (pending_delta + event.encoder_delta % (24 * 60)) % (24 * 60);
        }
    }

    if (!navigation_allowed || hg_navigator_is_sliding()) {
        return activity;
    }

    if (pending_toggle) {
        pending_toggle = false;
        hg_alarm_snapshot_t alarm;
        hg_alarm_get_snapshot(&alarm);
        hg_alarm_set_enabled(!alarm.enabled);
        hg_alarm_get_snapshot(&alarm);
        hg_clock_screen_update_alarm(&alarm);
        hg_effects_request_feedback(HG_FEEDBACK_ALARM_EDIT, millis());
    }

    hg_input_router_apply_encoder(pending_delta);
    pending_delta = 0;
    return activity;
}
