#include <Arduino.h>

#include "hg/config.h"
#include "hg/hal/encoder.h"
#include "hg/hal/touch.h"
#include "hg/log.h"
#include "runtime_internal.h"

void hg_input_capture_task(void *) {
    hg_runtime_wait_start();
    bool touch_ready = hg_touch_init();
    bool encoder_ready = hg_encoder_init();
    if (!touch_ready || !encoder_ready) {
        hg_runtime_input_fault();
        HG_ERROR("input", "Touch/encoder initialization failed\n");
    }
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t last_us = 0;
    uint32_t last_report_ms = 0;
    uint32_t last_overflow_ms = millis() - 1000U;
    uint32_t generation = 0;
    int32_t pending_delta = 0;
    bool encoder_down = false;
    bool touch_down = false;
    uint32_t button_sequence = 0;
    for (;;) {
        uint32_t now_us = micros();
        hg_touch_update();
        hg_encoder_update();
        hg_runtime_record_capture(last_us ? now_us - last_us : 0,
                                   hg_touch_is_pressed(), hg_encoder_button_is_pressed());
        last_us = now_us;
        hg_runtime_controls_t controls = hg_runtime_controls();
        if (controls.input_generation != generation) {
            generation = controls.input_generation;
            pending_delta = 0;
        }
        int32_t delta = hg_encoder_take_delta();
        int64_t sum = (int64_t)pending_delta + delta;
        pending_delta = sum > INT32_MAX ? INT32_MAX :
                        sum < INT32_MIN ? INT32_MIN : (int32_t)sum;
        bool next_encoder = hg_encoder_button_is_pressed();
        bool next_touch = hg_touch_is_pressed();
        bool encoder_changed = next_encoder != encoder_down;
        if (encoder_changed) {
            ++button_sequence;
        }
        hg_input_event_t event = {millis(), generation, hg_touch_tapped(),
                                  hg_encoder_button_pressed(), pending_delta,
                                  encoder_changed && !next_encoder,
                                  touch_down && !next_touch, button_sequence};
        encoder_down = next_encoder;
        touch_down = next_touch;
        hg_input_state_t state = {event.captured_ms, generation, button_sequence,
                                  encoder_down, touch_down};
        hg_runtime_capture_input_state(&state);
        bool publish = true;
#ifdef HG_RUNTIME_TEST
        publish = !hg_runtime_manual_input();
        if (!publish) {
            pending_delta = 0;
        }
#endif
        if (publish && (event.tapped || event.encoder_pressed || event.encoder_delta ||
                        event.encoder_released || event.touch_released)) {
            if (hg_runtime_publish_input(&event)) {
                pending_delta = 0;
            } else if (millis() - last_overflow_ms >= 1000U) {
                last_overflow_ms = millis();
                HG_ERROR("input", "Input queue overflow; see diagnostics\n");
            }
        }
        if (millis() - last_report_ms >= 1000U) {
            last_report_ms = millis();
            hg_runtime_record_worker(0, micros() - now_us);
        }
        hg_worker_delay(&last_wake, HG_INPUT_POLL_MS);
    }
}
