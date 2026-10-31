#include "hg/app/simulator.h"
#include "hg/app/runtime.h"
#include "hg/config.h"
#include "hg/log.h"

#include <deque>

static std::deque<hg_input_event_t> queue;
static hg_input_state_t state;
static uint32_t generation = 0;
static bool navigation = true;
static bool fault = false;
static bool indoor_paused = false;
static uint32_t policy_generation = 0;
static hg_runtime_diagnostics_t diagnostics;

bool hg_runtime_start(void) { return !fault; }
const char *hg_runtime_fault(void) { return fault ? "Simulated runtime fault" : nullptr; }
void hg_sim_set_fault(bool value) { fault = value; }
uint32_t hg_runtime_input_generation(void) { return generation; }
void hg_runtime_set_navigation_allowed(bool allowed) {
    if (allowed != navigation) {
        navigation = allowed;
        ++generation;
    }
}
void hg_sim_capture_state(void) {
    state.captured_ms = hg_sim_now();
    state.generation = generation;
}
void hg_sim_cancel_inputs(void) {
    queue.clear();
    if (state.encoder_down) ++state.button_sequence;
    state.encoder_down = state.touch_down = false;
    hg_sim_capture_state();
}

bool hg_sim_input(int kind, int32_t value) {
    hg_input_event_t event = {};
    hg_sim_capture_state();
    event.captured_ms = state.captured_ms;
    event.generation = generation;
    switch (kind) {
    case 0:
        if (state.encoder_down == (value != 0)) {
            HG_ERROR("sim", "Duplicate encoder transition\n");
            return false;
        }
        state.encoder_down = value != 0;
        ++state.button_sequence;
        event.encoder_pressed = state.encoder_down;
        event.encoder_released = !state.encoder_down;
        break;
    case 1:
        if (state.touch_down == (value != 0)) {
            HG_ERROR("sim", "Duplicate touch transition\n");
            return false;
        }
        state.touch_down = value != 0;
        event.tapped = state.touch_down;
        event.touch_released = !state.touch_down;
        break;
    case 2:
        event.encoder_delta = value;
        break;
    default:
        HG_ERROR("sim", "Unknown input kind\n");
        return false;
    }
    event.button_sequence = state.button_sequence;
    if (queue.size() >= HG_INPUT_QUEUE_CAPACITY) {
        ++diagnostics.input_overflows;
        HG_ERROR("sim", "Input queue overflow\n");
        return false;
    }
    queue.push_back(event);
    if (queue.size() > diagnostics.input_queue_high_water) {
        diagnostics.input_queue_high_water = queue.size();
    }
    return true;
}

bool hg_runtime_take_input(hg_input_event_t *out, uint32_t now) {
    if (queue.empty()) {
        return false;
    }
    *out = queue.front();
    queue.pop_front();
    if (now - out->captured_ms > HG_INPUT_MAX_AGE_MS) {
        ++diagnostics.expired_inputs;
        out->tapped = out->encoder_pressed = out->encoder_released = out->touch_released = false;
        out->encoder_delta = 0;
    }
    return true;
}
bool hg_runtime_take_input(hg_input_event_t *out) {
    return hg_runtime_take_input(out, hg_sim_now());
}
void hg_runtime_get_input_state(hg_input_state_t *out) { *out = state; }
uint32_t hg_runtime_set_indoor_paused(bool paused) {
    if (paused != indoor_paused) {
        indoor_paused = paused;
        ++policy_generation;
    }
    return policy_generation;
}
bool hg_runtime_audio_running(void) { return false; }
void hg_runtime_set_audio_command(const hg_audio_command_t *) {
    HG_ERROR("sim", "Audio output is unavailable in the browser\n");
}
bool hg_runtime_get_audio_status(hg_audio_status_t *) { return false; }
void hg_runtime_record_ui(uint32_t, uint32_t, bool) {}
void hg_runtime_get_diagnostics(hg_runtime_diagnostics_t *out) {
    *out = diagnostics;
    out->encoder_pressed = state.encoder_down;
    out->touch_pressed = state.touch_down;
}
void hg_runtime_reset_diagnostics(void) { diagnostics = {}; }
