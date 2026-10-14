#include "hg/hal/touch.h"

#include <Arduino.h>
#include <driver/touch_sensor.h>
#include <esp_err.h>
#include <soc/rtc_cntl_reg.h>

#include "hg/config.h"
#include "hg/hal/pins.h"
#include "hg/log.h"

static bool touch_ready = false;
static uint32_t touch_baseline = 0;
static uint32_t touch_threshold = 0;
static bool touch_last_stable = false;
static bool touch_last_raw = false;
static uint32_t touch_last_change_ms = 0;
static bool touch_tapped_edge = false;
static touch_pad_t touch_channel = TOUCH_PAD_MAX;
static uint32_t touch_raw = 0;
static uint32_t touch_last_sample_ms = 0;
static uint32_t touch_fresh_samples = 0;
static uint32_t touch_timeouts = 0;
static uint32_t touch_last_error_ms = 0;
static bool touch_hardware_fault = false;

static bool hg_touch_check(esp_err_t result, const char *operation) {
    if (result == ESP_OK) {
        return true;
    }
    HG_ERROR("touch", "%s failed: %s\n", operation, esp_err_to_name(result));
    touch_hardware_fault = true;
    touch_ready = false;
    touch_last_stable = false;
    touch_tapped_edge = false;
    return false;
}

#if HG_LOG_TOUCH_DEBUG
static uint32_t log_last_ms = 0;
static uint32_t log_min = UINT32_MAX;
static uint32_t log_max = 0;
static uint32_t log_high_samples = 0;
static uint32_t log_samples = 0;
static uint32_t log_taps = 0;

static void hg_touch_log_sample(uint32_t raw, uint32_t now) {
    if (raw < log_min) {
        log_min = raw;
    }
    if (raw > log_max) {
        log_max = raw;
    }
    ++log_samples;
    if (raw >= touch_threshold) {
        ++log_high_samples;
    }
    if (now - log_last_ms >= 500U) {
        HG_TOUCH_LOG("touch", "raw=%lu min=%lu max=%lu thr=%lu hi=%lu/%lu down=%d taps=%lu\n",
                     (unsigned long)raw, (unsigned long)log_min, (unsigned long)log_max,
                     (unsigned long)touch_threshold, (unsigned long)log_high_samples,
                     (unsigned long)log_samples, touch_last_stable ? 1 : 0,
                     (unsigned long)log_taps);
        HG_TOUCH_LOG("touch", "fresh=%lu age=%lu ms timeouts=%lu\n",
                     (unsigned long)touch_fresh_samples,
                     (unsigned long)(now - touch_last_sample_ms),
                     (unsigned long)touch_timeouts);
        log_last_ms = now;
        log_min = UINT32_MAX;
        log_max = 0;
        log_high_samples = 0;
        log_samples = 0;
    }
}
#endif

static bool hg_touch_sample(uint32_t now) {
    // Poll raw flags: interrupts stay disabled, so no ISR can consume completion.
    uint32_t status = REG_READ(RTC_CNTL_INT_RAW_REG);
    if (status & RTC_CNTL_TOUCH_TIMEOUT_INT_RAW) {
        ++touch_timeouts;
        if (touch_timeouts == 1 || now - touch_last_error_ms >= 1000U) {
            HG_ERROR("touch", "measurement timeout channel=%d; resuming scan\n",
                     (int)touch_pad_get_current_meas_channel());
            touch_last_error_ms = now;
        }
        if (hg_touch_check(touch_pad_intr_clear(TOUCH_PAD_INTR_MASK_ALL), "clear timeout")) {
            hg_touch_check(touch_pad_timeout_resume(), "resume timeout");
        }
        touch_last_raw = touch_last_stable;
        touch_last_change_ms = now;
        return false;
    }
    if (!(status & RTC_CNTL_TOUCH_SCAN_DONE_INT_RAW)) {
        return false;
    }
    if (!hg_touch_check(touch_pad_intr_clear(TOUCH_PAD_INTR_MASK_SCAN_DONE), "clear scan") ||
        !hg_touch_check(touch_pad_read_raw_data(touch_channel, &touch_raw), "read raw")) {
        return false;
    }
    ++touch_fresh_samples;
    touch_last_sample_ms = now;
    return true;
}

bool hg_touch_init(void) {
    touch_ready = false;
    int channel = digitalPinToTouchChannel(HG_PIN_TOUCH);
    if (channel <= TOUCH_PAD_NUM0 || channel >= TOUCH_PAD_MAX) {
        HG_ERROR("touch", "GPIO %d is not a usable touch channel\n", HG_PIN_TOUCH);
        return false;
    }
    touch_channel = static_cast<touch_pad_t>(channel);
    touch_raw = 0;
    touch_fresh_samples = 0;
    touch_timeouts = 0;
    touch_last_error_ms = 0;
    touch_hardware_fault = false;

    if (!hg_touch_check(touch_pad_init(), "initialize")) {
        return false;
    }

    // Configure the channel before starting the FSM, unlike Arduino touchRead().
    touch_pad_denoise_t denoise = {};
    denoise.grade = TOUCH_PAD_DENOISE_BIT4;
    denoise.cap_level = TOUCH_PAD_DENOISE_CAP_L4;
    if (!hg_touch_check(touch_pad_config(touch_channel), "configure channel") ||
        !hg_touch_check(touch_pad_denoise_set_config(&denoise), "configure denoise") ||
        !hg_touch_check(touch_pad_denoise_enable(), "enable denoise") ||
        !hg_touch_check(touch_pad_timeout_set(true, RTC_CNTL_TOUCH_TIMEOUT_NUM_V),
                        "configure timeout") ||
        !hg_touch_check(touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER), "set scan mode") ||
        !hg_touch_check(touch_pad_fsm_start(), "start scan")) {
        hg_touch_check(touch_pad_deinit(), "cleanup");
        return false;
    }

    uint64_t sum = 0;
    uint32_t samples = 0;
    uint32_t started = millis();
    touch_last_sample_ms = started;
    while (samples < HG_TOUCH_BASELINE_SAMPLES && millis() - started < 1000U) {
        uint32_t timeouts = touch_timeouts;
        if (hg_touch_sample(millis())) {
            sum += touch_raw;
            ++samples;
        } else if (touch_timeouts != timeouts) {
            sum = 0;
            samples = 0;
        }
        if (touch_hardware_fault) {
            break;
        }
        delay(2);
    }
    if (samples < HG_TOUCH_BASELINE_SAMPLES) {
        HG_ERROR("touch", "calibration failed: fresh=%lu/%u timeouts=%lu\n",
                 (unsigned long)samples, HG_TOUCH_BASELINE_SAMPLES,
                 (unsigned long)touch_timeouts);
        hg_touch_check(touch_pad_deinit(), "cleanup");
        return false;
    }

    touch_baseline = sum / HG_TOUCH_BASELINE_SAMPLES;
    touch_threshold = touch_baseline + HG_TOUCH_THRESHOLD_DELTA;

    touch_last_stable = false;
    touch_last_raw = false;
    touch_last_change_ms = millis();
    touch_tapped_edge = false;
    touch_ready = true;

#if HG_LOG_TOUCH_DEBUG
    log_last_ms = millis();
    log_min = UINT32_MAX;
    log_max = 0;
    log_high_samples = 0;
    log_samples = 0;
    log_taps = 0;
#endif
    HG_TOUCH_LOG("touch", "init pin=%d baseline=%lu threshold=%lu debounce=%u ms\n",
                 HG_PIN_TOUCH, (unsigned long)touch_baseline,
                 (unsigned long)touch_threshold, HG_TOUCH_DEBOUNCE_MS);
    return true;
}

void hg_touch_update(void) {
    if (!touch_ready) {
        return;
    }

    uint32_t now = millis();
    if (!hg_touch_sample(now)) {
        if (now - touch_last_sample_ms >= HG_TOUCH_DEBOUNCE_MS) {
            touch_last_raw = touch_last_stable;
            touch_last_change_ms = now;
        }
        if (now - touch_last_sample_ms >= 250U && now - touch_last_error_ms >= 1000U) {
            HG_ERROR("touch", "no completed scan for %lu ms; timeouts=%lu\n",
                     (unsigned long)(now - touch_last_sample_ms),
                     (unsigned long)touch_timeouts);
            touch_last_error_ms = now;
        }
#if HG_LOG_TOUCH_DEBUG
        hg_touch_log_sample(touch_raw, now);
#endif
        return;
    }
    uint32_t raw = touch_raw;
    bool raw_pressed = raw >= touch_threshold;

    if (raw_pressed != touch_last_raw) {
        touch_last_raw = raw_pressed;
        touch_last_change_ms = now;
    }
    if (raw_pressed != touch_last_stable &&
        (now - touch_last_change_ms) >= HG_TOUCH_DEBOUNCE_MS) {
        touch_last_stable = raw_pressed;
        if (raw_pressed) {
            touch_tapped_edge = true;
#if HG_LOG_TOUCH_DEBUG
            ++log_taps;
#endif
        }
        HG_TOUCH_LOG("touch", "%s raw=%lu\n", raw_pressed ? "PRESSED" : "RELEASED",
                     (unsigned long)raw);
    }
#if HG_LOG_TOUCH_DEBUG
    hg_touch_log_sample(raw, now);
#endif
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
