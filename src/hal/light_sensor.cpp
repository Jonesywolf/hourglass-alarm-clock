#include "hg/hal/light_sensor.h"

#include <Adafruit_TSL2591.h>
#include <Arduino.h>
#include <Wire.h>

#include "hg/hal/pins.h"

#define HG_TSL_AVALID 0x01U

static Adafruit_TSL2591 sensor(2591);
static bool sensor_ready = false;

static uint8_t hg_tsl_read8(uint8_t reg) {
    Wire.beginTransmission(TSL2591_ADDR);
    Wire.write((uint8_t)(TSL2591_COMMAND_BIT | reg));
    if (Wire.endTransmission() != 0) {
        return 0;
    }
    if (Wire.requestFrom((uint8_t)TSL2591_ADDR, (uint8_t)1) != 1) {
        return 0;
    }
    return (uint8_t)Wire.read();
}

static uint16_t hg_tsl_read16(uint8_t reg) {
    Wire.beginTransmission(TSL2591_ADDR);
    Wire.write((uint8_t)(TSL2591_COMMAND_BIT | reg));
    if (Wire.endTransmission() != 0) {
        return 0;
    }
    if (Wire.requestFrom((uint8_t)TSL2591_ADDR, (uint8_t)2) != 2) {
        return 0;
    }
    uint16_t lo = (uint16_t)Wire.read();
    uint16_t hi = (uint16_t)Wire.read();
    return (uint16_t)((hi << 8) | lo);
}

bool hg_light_sensor_init(void) {
    Wire.begin(HG_PIN_TSL2591_SDA, HG_PIN_TSL2591_SCL);

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

bool hg_light_sensor_read_lux(float *lux) {
    if (!lux) {
        return false;
    }

    *lux = 0.0f;
    if (!sensor_ready) {
        return false;
    }

    uint8_t status = hg_tsl_read8(TSL2591_REGISTER_DEVICE_STATUS);
    if ((status & HG_TSL_AVALID) == 0) {
        return false;
    }

    /* CHAN0 must be read before CHAN1. */
    uint16_t full = hg_tsl_read16(TSL2591_REGISTER_CHAN0_LOW);
    uint16_t ir = hg_tsl_read16(TSL2591_REGISTER_CHAN1_LOW);
    float value = sensor.calculateLux(full, ir);
    if (value < 0.0f || !isfinite(value)) {
        return false;
    }

    *lux = value;
    return true;
}
