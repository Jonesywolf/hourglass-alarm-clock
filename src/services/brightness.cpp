#include "hg/services/brightness.h"

#include "hg/config.h"
#include "hg/hal/display.h"
#include "hg/hal/light_sensor.h"

static uint8_t percent = 100;
static hg_light_level_t light_level = HG_LIGHT_NEUTRAL;

void hg_brightness_init(void) {
    percent = 100;
    light_level = HG_LIGHT_NEUTRAL;
    hg_light_sensor_init();
    hg_display_set_brightness(percent);
}

void hg_brightness_update(void) {
    float lux = 0.0f;
    if (hg_light_sensor_read_lux(&lux)) {
        if (lux >= HG_DISPLAY_BRIGHT_LUX) {
            light_level = HG_LIGHT_BRIGHT;
        } else if (lux <= HG_DISPLAY_DARK_LUX) {
            light_level = HG_LIGHT_DARK;
        } else {
            light_level = HG_LIGHT_NEUTRAL;
        }
    }
    hg_display_set_brightness(percent);
}

hg_light_level_t hg_brightness_get_light_level(void) {
    return light_level;
}

uint8_t hg_brightness_get_percent(void) {
    return percent;
}

