#include "hg/hal/encoder.h"

#include <Arduino.h>

#include "hg/hal/pins.h"
#include "encoder_internal.h"

static const uint32_t HG_ENC_BTN_DEBOUNCE_MS = 30;

static volatile uint8_t enc_state = HG_ENC_R_START;
static volatile int32_t enc_delta = 0;
static portMUX_TYPE enc_mux = portMUX_INITIALIZER_UNLOCKED;
static bool enc_ready = false;

static bool enc_btn_last_stable = false;
static bool enc_btn_last_raw = false;
static uint32_t enc_btn_last_change_ms = 0;
static bool enc_btn_pressed_edge = false;

static void IRAM_ATTR hg_encoder_isr(void) {
    portENTER_CRITICAL_ISR(&enc_mux);
    uint8_t pin_state = (uint8_t)((digitalRead(HG_PIN_ENC_A) << 1) | digitalRead(HG_PIN_ENC_B));
    enc_state = hg_encoder_table[enc_state & 0x0F][pin_state];
    uint8_t direction = enc_state & 0x30;
    if (direction == HG_ENC_DIR_CW) {
        if (enc_delta < INT32_MAX) {
            enc_delta++;
        }
    } else if (direction == HG_ENC_DIR_CCW) {
        if (enc_delta > INT32_MIN) {
            enc_delta--;
        }
    }
    portEXIT_CRITICAL_ISR(&enc_mux);
}

bool hg_encoder_init(void) {
    if (HG_PIN_ENC_A < 0 || HG_PIN_ENC_B < 0) {
        enc_ready = false;
        return false;
    }

    pinMode(HG_PIN_ENC_A, INPUT_PULLUP);
    pinMode(HG_PIN_ENC_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(HG_PIN_ENC_A), hg_encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(HG_PIN_ENC_B), hg_encoder_isr, CHANGE);

    if (HG_PIN_ENC_BTN >= 0) {
        pinMode(HG_PIN_ENC_BTN, INPUT_PULLUP);
        enc_btn_last_stable = (digitalRead(HG_PIN_ENC_BTN) == LOW);
        enc_btn_last_raw = enc_btn_last_stable;
        enc_btn_last_change_ms = millis();
    }

    enc_ready = true;
    return true;
}

void hg_encoder_update(void) {
    if (!enc_ready || HG_PIN_ENC_BTN < 0) {
        return;
    }

    bool raw_pressed = (digitalRead(HG_PIN_ENC_BTN) == LOW);
    uint32_t now = millis();

    if (raw_pressed != enc_btn_last_raw) {
        enc_btn_last_raw = raw_pressed;
        enc_btn_last_change_ms = now;
    }
    if (raw_pressed != enc_btn_last_stable && (now - enc_btn_last_change_ms) >= HG_ENC_BTN_DEBOUNCE_MS) {
        enc_btn_last_stable = raw_pressed;
        if (raw_pressed) {
            enc_btn_pressed_edge = true;
        }
    }
}

int32_t hg_encoder_take_delta(void) {
    if (!enc_ready) {
        return 0;
    }

    portENTER_CRITICAL(&enc_mux);
    int32_t delta = enc_delta;
    enc_delta = 0;
    portEXIT_CRITICAL(&enc_mux);
    return delta;
}

bool hg_encoder_button_pressed(void) {
    if (!enc_btn_pressed_edge) {
        return false;
    }

    enc_btn_pressed_edge = false;
    return true;
}

bool hg_encoder_button_is_pressed(void) {
    return enc_ready && enc_btn_last_stable;
}
