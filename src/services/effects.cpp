#include "hg/services/effects.h"

#include "hg/config.h"

static hg_effects_snapshot_t state;
static bool pause_for_leds = false;
static bool feedback_seen = false;
static uint32_t feedback_started_ms = 0;

static void refresh_resources(void) {
    state.pause_indoor = pause_for_leds && (state.leds_active || state.leds_starting);
    state.timing_sensitive = state.phase != HG_EFFECT_NORMAL || state.leds_starting ||
                             state.audio_active || state.feedback != HG_FEEDBACK_NONE ||
                             state.stop_pending;
}

static void stop_feedback(void) {
    if (state.feedback == HG_FEEDBACK_NONE) {
        return;
    }
    state.feedback = HG_FEEDBACK_NONE;
    ++state.generation;
    state.stop_pending = state.led_available || state.audio_available ||
                         state.leds_active || state.audio_active;
    refresh_resources();
}

void hg_effects_init(void) {
    state = {};
    state.generation = 1;
    pause_for_leds = false;
    feedback_seen = false;
    feedback_started_ms = 0;
}

void hg_effects_set_capabilities(bool led, bool audio) {
    state.led_available = led;
    state.audio_available = audio;
    refresh_resources();
}

void hg_effects_set_alarm(bool sunrise, bool ringing) {
    hg_effect_phase_t next = ringing ? HG_EFFECT_RINGING :
                             sunrise ? HG_EFFECT_SUNRISE : HG_EFFECT_NORMAL;
    if (next == state.phase) {
        return;
    }
    state.phase = next;
    state.feedback = HG_FEEDBACK_NONE;
    ++state.generation;
    state.stop_pending = next == HG_EFFECT_NORMAL &&
                         (state.led_available || state.audio_available ||
                          state.leds_active || state.audio_active || state.leds_starting);
    refresh_resources();
}

void hg_effects_set_pause_indoor_for_leds(bool enabled) {
    pause_for_leds = enabled;
    refresh_resources();
}

hg_feedback_result_t hg_effects_request_feedback(hg_feedback_t feedback, uint32_t now_ms) {
    if (state.phase != HG_EFFECT_NORMAL || state.stop_pending) {
        return HG_FEEDBACK_PREEMPTED;
    }
    if (feedback == HG_FEEDBACK_NONE || (!state.led_available && !state.audio_available)) {
        return HG_FEEDBACK_UNAVAILABLE;
    }
    if (feedback_seen && now_ms - feedback_started_ms < HG_FEEDBACK_INTERVAL_MS) {
        return HG_FEEDBACK_RATE_LIMITED;
    }
    state.feedback = feedback;
    feedback_started_ms = now_ms;
    feedback_seen = true;
    ++state.generation;
    refresh_resources();
    return HG_FEEDBACK_ACCEPTED;
}

void hg_effects_update(uint32_t now_ms) {
    if (state.feedback != HG_FEEDBACK_NONE &&
        now_ms - feedback_started_ms >= HG_FEEDBACK_DURATION_MS) {
        stop_feedback();
    }
}

void hg_effects_cancel_feedback(void) {
    stop_feedback();
}

bool hg_effects_report_applied(uint32_t generation, bool leds_active,
                               bool audio_active, bool leds_starting) {
    if (generation != state.generation ||
        ((leds_active || leds_starting) && !state.led_available) ||
        (audio_active && !state.audio_available) ||
        (state.stop_pending && (leds_active || audio_active || leds_starting))) {
        return false;
    }
    state.leds_active = leds_active;
    state.leds_starting = leds_starting;
    state.audio_active = audio_active;
    if (!leds_active && !audio_active && !leds_starting) {
        state.stop_pending = false;
    }
    refresh_resources();
    return true;
}

void hg_effects_get_snapshot(hg_effects_snapshot_t *out) {
    if (out) {
        *out = state;
    }
}
