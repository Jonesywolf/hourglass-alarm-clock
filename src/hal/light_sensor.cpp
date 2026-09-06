#include "hg/hal/light_sensor.h"

#include <Adafruit_TSL2591.h>
#include <Arduino.h>
#include <Wire.h>

#include "hg/hal/pins.h"

static Adafruit_TSL2591 sensor(2591);
static bool sensor_ready = false;

bool hg_light_sensor_init(void) {
    Wire.begin(HG_PIN_TSL2591_SDA, HG_PIN_TSL2591_SCL);

    if (!sensor.begin(&Wire)) {
        sensor_ready = false;
        return false;
    }

    sensor.setGain(TSL2591_GAIN_MED);
    sensor.setTiming(TSL2591_INTEGRATIONTIME_100MS);
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

    sensors_event_t event;
    sensor.getEvent(&event);
    if (event.light < 0.0f || !isfinite(event.light)) {
        return false;
    }

    *lux = event.light;
    return true;
}
