#include "hg/hal/light_sensor.h"

#include <Adafruit_TSL2591.h>
#include <Arduino.h>
#include <Wire.h>

#define HG_TSL_AVALID 0x01U

static Adafruit_TSL2591 sensor(2591);
static bool sensor_ready = false;

static bool hg_tsl_read8(uint8_t reg, uint8_t *out) {
    Wire.beginTransmission(TSL2591_ADDR);
    Wire.write((uint8_t)(TSL2591_COMMAND_BIT | reg));
    if (Wire.endTransmission() != 0) {
        return false;
    }
    if (Wire.requestFrom((uint8_t)TSL2591_ADDR, (uint8_t)1) != 1) {
        return false;
    }
    *out = (uint8_t)Wire.read();
    return true;
}

static bool hg_tsl_read16(uint8_t reg, uint16_t *out) {
    Wire.beginTransmission(TSL2591_ADDR);
    Wire.write((uint8_t)(TSL2591_COMMAND_BIT | reg));
    if (Wire.endTransmission() != 0) {
        return false;
    }
    if (Wire.requestFrom((uint8_t)TSL2591_ADDR, (uint8_t)2) != 2) {
        return false;
    }
    uint16_t lo = (uint16_t)Wire.read();
    uint16_t hi = (uint16_t)Wire.read();
    *out = (uint16_t)((hi << 8) | lo);
    return true;
}

bool hg_light_sensor_init(void) {
    if (!sensor.begin(&Wire)) {
        sensor_ready = false;
        return false;
    }

    sensor.setGain(TSL2591_GAIN_MED);
    sensor.setTiming(TSL2591_INTEGRATIONTIME_100MS);
    /* Adafruit setTiming() powers the chip off. Leave ALS running so later
     * reads are I2C-only; getEvent() would delay(120) twice (~244 ms). */
    sensor.enable();
    sensor_ready = true;
    return true;
}

hg_sample_status_t hg_light_sensor_read_lux(float *lux) {
    if (!lux) {
        return HG_SAMPLE_ERROR;
    }

    if (!sensor_ready) {
        return HG_SAMPLE_ERROR;
    }

    uint8_t status = 0;
    if (!hg_tsl_read8(TSL2591_REGISTER_DEVICE_STATUS, &status)) {
        return HG_SAMPLE_ERROR;
    }
    if ((status & HG_TSL_AVALID) == 0) {
        return HG_SAMPLE_WAITING;
    }

    /* CHAN0 must be read before CHAN1. */
    uint16_t full = 0;
    uint16_t ir = 0;
    if (!hg_tsl_read16(TSL2591_REGISTER_CHAN0_LOW, &full) ||
        !hg_tsl_read16(TSL2591_REGISTER_CHAN1_LOW, &ir)) {
        return HG_SAMPLE_ERROR;
    }
    float value = sensor.calculateLux(full, ir);
    if (value < 0.0f || !isfinite(value)) {
        return HG_SAMPLE_ERROR;
    }

    *lux = value;
    return HG_SAMPLE_READY;
}
