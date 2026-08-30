#include "hg/services/brightness.h"

#include "hg/hal/display.h"
#include "hg/hal/light_sensor.h"

static uint8_t percent = 100;
static bool nightlight = false;

void hg_brightness_init(void) {
    percent = 100;
    nightlight = false;
    hg_light_sensor_init();
    hg_display_set_brightness(percent);
}

void hg_brightness_update(void) {
    float lux = 0.0f;
    if (hg_light_sensor_read_lux(&lux)) {
        (void)lux;
    }
    hg_display_set_brightness(percent);
}

void hg_brightness_set_nightlight(bool enabled) {
    nightlight = enabled;
}

uint8_t hg_brightness_get_percent(void) {
    (void)nightlight;
    return percent;
}
