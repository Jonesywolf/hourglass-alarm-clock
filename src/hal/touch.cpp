#include "hg/hal/touch.h"

#include <Arduino.h>

#include "hg/config.h"
#include "hg/hal/pins.h"
#include "hg/log.h"

static bool touch_ready = false;
static uint32_t touch_baseline = 0;
static uint32_t touch_threshold = 0;
static bool touch_last_stable = false;
static uint32_t touch_last_change_ms = 0;
static bool touch_tapped_edge = false;

static uint32_t hg_touch_sample(void) {
    return (uint32_t)touchRead(HG_PIN_TOUCH);
}

bool hg_touch_init(void) {
    if (HG_PIN_TOUCH < 0) {
        touch_ready = false;
        return false;
    }

    delay(20);

    uint32_t sum = 0;
    for (uint32_t i = 0; i < HG_TOUCH_BASELINE_SAMPLES; i++) {
        sum += hg_touch_sample();
        delay(2);
    }

    touch_baseline = sum / HG_TOUCH_BASELINE_SAMPLES;
    touch_threshold = touch_baseline + HG_TOUCH_THRESHOLD_DELTA;

    touch_last_stable = false;
    touch_last_change_ms = millis();
    touch_tapped_edge = false;
    touch_ready = true;

    HG_LOG("touch", "baseline=%u threshold=%u\n", (unsigned)touch_baseline,
           (unsigned)touch_threshold);
    return true;
}

void hg_touch_update(void) {
    if (!touch_ready) {
        return;
    }

    bool raw_pressed = hg_touch_sample() >= touch_threshold;
    uint32_t now = millis();

    if (raw_pressed != touch_last_stable &&
        (now - touch_last_change_ms) >= HG_TOUCH_DEBOUNCE_MS) {
        touch_last_change_ms = now;
        touch_last_stable = raw_pressed;
        if (raw_pressed) {
            touch_tapped_edge = true;
        }
    }
}

bool hg_touch_is_pressed(void) {
    return touch_ready && touch_last_stable;
}

bool hg_touch_tapped(void) {
    if (!touch_tapped_edge) {
        return false;
    }
    touch_tapped_edge = false;
    return true;
}
