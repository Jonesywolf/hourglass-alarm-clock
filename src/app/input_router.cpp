#include "hg/app/input_router.h"

#include <Arduino.h>

#include "hg/app/runtime.h"
#include "hg/config.h"
#include "hg/services/alarm.h"
#include "hg/services/display_mode.h"
#include "hg/services/effects.h"
#include "hg/ui/navigator.h"

static hg_alarm_edit_snapshot_t edit;
static uint32_t last_encoder_ms = 0;
static uint32_t pressed_ms = 0;
static uint32_t button_sequence = 0;
static uint32_t input_generation = 0;
static hg_page_id_t pressed_page = HG_PAGE_CLOCK;
static bool button_down = false;
static bool consume_button = false;
static bool consume_touch = false;
static bool press_allowed = false;

static bool editing(void) {
    return edit.field != HG_ALARM_EDIT_NONE;
}

static void finish_edit(uint32_t now) {
    hg_alarm_set_time(edit.hour, edit.minute);
    edit.field = HG_ALARM_EDIT_NONE;
    hg_alarm_set_editing(false);
    hg_effects_request_feedback(HG_FEEDBACK_ALARM_SAVE, now);
}

static void hold_action(uint32_t now) {
    if (consume_button || !press_allowed || !hg_alarm_toggle_allowed(now)) {
        return;
    }
    consume_button = true;
    if (editing()) {
        finish_edit(now);
    } else {
        hg_alarm_snapshot_t alarm;
        hg_alarm_get_snapshot(&alarm);
        edit = {HG_ALARM_EDIT_MINUTE, alarm.hour, alarm.minute};
        hg_alarm_set_editing(true);
        hg_effects_request_feedback(HG_FEEDBACK_ALARM_ENTER, now);
    }
    last_encoder_ms = now;
}

static void short_action(uint32_t now) {
    if (consume_button || !press_allowed || !hg_alarm_toggle_allowed(now)) {
        return;
    }
    if (editing()) {
        edit.field = edit.field == HG_ALARM_EDIT_MINUTE ? HG_ALARM_EDIT_HOUR :
                                                        HG_ALARM_EDIT_MINUTE;
        hg_effects_request_feedback(HG_FEEDBACK_ALARM_FIELD, now);
    } else {
        hg_alarm_snapshot_t alarm;
        hg_alarm_get_snapshot(&alarm);
        hg_alarm_set_enabled(!alarm.enabled);
        hg_effects_request_feedback(alarm.enabled ? HG_FEEDBACK_ALARM_DISABLE :
                                                    HG_FEEDBACK_ALARM_ENABLE, now);
    }
    last_encoder_ms = now;
}

static void clock_rotation(int32_t delta, uint32_t now) {
    if (!editing()) {
        /* Idle clock rotation is reserved for future volume control. */
        return;
    }
    int minutes = edit.hour * 60 + edit.minute;
    minutes += (int)(delta % (24 * 60)) *
               (edit.field == HG_ALARM_EDIT_HOUR ? 60 : 1);
    minutes = (minutes % (24 * 60) + 24 * 60) % (24 * 60);
    edit.hour = minutes / 60;
    edit.minute = minutes % 60;
    last_encoder_ms = now;
    hg_effects_request_feedback(HG_FEEDBACK_ALARM_EDIT, now);
}

void hg_input_router_init(void) {
    if (editing()) {
        hg_alarm_set_editing(false);
    }
    edit = {};
    last_encoder_ms = pressed_ms = button_sequence = input_generation = 0;
    button_down = consume_button = consume_touch = press_allowed = false;
}

void hg_input_router_get_alarm_edit(hg_alarm_edit_snapshot_t *out) {
    *out = edit;
}

static bool update(bool navigation_allowed, uint32_t now, bool realtime) {
    bool activity = false;
    uint32_t generation = hg_runtime_input_generation();
    bool clock_allowed = navigation_allowed && !hg_navigator_is_sliding() &&
                         hg_navigator_current() == HG_PAGE_CLOCK;
    if (generation != input_generation || !clock_allowed ||
        hg_navigator_current() != pressed_page) {
        consume_button = button_down;
        press_allowed = false;
        input_generation = generation;
    }
    if (editing() && !clock_allowed) {
        finish_edit(now);
    }

    for (unsigned i = 0; i < HG_INPUT_QUEUE_CAPACITY; ++i) {
        hg_input_event_t event;
        bool received = realtime ? hg_runtime_take_input(&event) :
                                   hg_runtime_take_input(&event, now);
        if (!received) {
            break;
        }
        if (realtime) {
            now = millis();
        }
        bool fresh = now - event.captured_ms <= HG_INPUT_MAX_AGE_MS;
        bool allowed = navigation_allowed && event.generation == generation && fresh;
        bool input = event.tapped || event.encoder_pressed || event.encoder_delta != 0;
        activity = activity || input || event.encoder_released || event.touch_released;
        if (!fresh) {
            consume_button = button_down;
            press_allowed = false;
            continue;
        }

        hg_alarm_snapshot_t alarm;
        hg_alarm_get_snapshot(&alarm);
        bool was_editing = editing();
        bool woke = false;
        if (input && event.generation == generation) {
            woke = hg_display_mode_note_activity(now);
        }
        if (woke) {
            consume_button = consume_button || button_down || event.encoder_pressed;
            consume_touch = consume_touch || event.tapped;
            press_allowed = false;
        }
        if (event.encoder_pressed || event.encoder_released) {
            bool sequential = event.button_sequence == button_sequence + 1;
            button_sequence = event.button_sequence;
            if (event.encoder_pressed) {
                button_down = true;
                pressed_ms = event.captured_ms;
                pressed_page = hg_navigator_current();
                consume_button = woke || !sequential;
                press_allowed = allowed && !hg_navigator_is_sliding() &&
                                pressed_page == HG_PAGE_CLOCK;
                if (alarm.ringing || alarm.snoozed) {
                    hg_alarm_dismiss();
                    hg_effects_set_alarm(false, false);
                    consume_button = true;
                    press_allowed = false;
                }
                if (editing()) {
                    last_encoder_ms = now;
                }
            } else {
                if (button_down && sequential && allowed && !alarm.ringing && !alarm.snoozed &&
                    !hg_navigator_is_sliding() && pressed_page == hg_navigator_current()) {
                    if (event.captured_ms - pressed_ms >= HG_ENCODER_HOLD_MS) {
                        hold_action(now);
                    } else {
                        short_action(now);
                    }
                }
                button_down = false;
                consume_button = press_allowed = false;
                if (editing()) {
                    last_encoder_ms = now;
                }
            }
        }
        if (event.tapped && !woke && !consume_touch && !was_editing && !editing()) {
            if (alarm.ringing) {
                hg_alarm_snooze();
                hg_effects_set_alarm(false, false);
            } else if (allowed) {
                hg_navigator_next();
                hg_effects_request_feedback(HG_FEEDBACK_NAVIGATE, now);
                consume_button = button_down;
                press_allowed = false;
            }
        }
        if (event.touch_released) {
            consume_touch = false;
        }
        if (event.encoder_delta && !woke && !consume_touch && allowed &&
            !hg_navigator_is_sliding() && !alarm.ringing && !alarm.snoozed &&
            !(button_down && consume_button && !editing())) {
            switch (hg_navigator_current()) {
                case HG_PAGE_CLOCK:
                    clock_rotation(event.encoder_delta, now);
                    break;
                default:
                    break;
            }
        }
    }

    hg_input_state_t state;
    hg_runtime_get_input_state(&state);
    if (realtime) {
        now = millis();
    }
    bool state_fresh = state.generation == generation &&
                       now - state.captured_ms <= HG_INPUT_MAX_AGE_MS;
    if (!state_fresh || state.button_sequence != button_sequence ||
        state.encoder_down != button_down) {
        /* Lost/expired transitions cancel gestures; current state is never a new press. */
        button_sequence = state.button_sequence;
        button_down = state.encoder_down;
        consume_button = button_down;
        press_allowed = false;
    }
    if (state_fresh && !state.touch_down) {
        consume_touch = false;
    }
    if (state_fresh && (state.encoder_down || state.touch_down)) {
        if (hg_display_mode_note_activity(now)) {
            consume_button = button_down;
            consume_touch = state.touch_down;
            press_allowed = false;
        }
        activity = true;
    }

    hg_alarm_snapshot_t alarm;
    hg_alarm_get_snapshot(&alarm);
    if (button_down && state_fresh && press_allowed &&
        navigation_allowed && !hg_navigator_is_sliding() &&
        hg_navigator_current() == pressed_page && !alarm.ringing && !alarm.snoozed &&
        now - pressed_ms >= HG_ENCODER_HOLD_MS) {
        hold_action(now);
    }
    if (editing()) {
        if (button_down && state_fresh) {
            last_encoder_ms = now;
        } else if (now - last_encoder_ms >= HG_ALARM_EDIT_IDLE_MS) {
            finish_edit(now);
        }
    }
    return activity;
}

bool hg_input_router_update(bool navigation_allowed) {
    return update(navigation_allowed, millis(), true);
}

#ifdef HG_RUNTIME_TEST
bool hg_input_router_test_update(bool navigation_allowed, uint32_t now_ms) {
    return update(navigation_allowed, now_ms, false);
}
#endif
