#include "hg/services/brightness.h"

#include "hg/config.h"
#include "hg/hal/display.h"

static uint8_t percent = 100;
static hg_light_level_t light_level = HG_LIGHT_NEUTRAL;

void hg_brightness_init(void) {
    percent = 100;
    light_level = HG_LIGHT_NEUTRAL;
    hg_display_set_brightness(percent);
}

void hg_brightness_update(const hg_light_snapshot_t *light) {
    if (light && light->valid && light->status == HG_SAMPLE_READY) {
        float lux = light->lux;
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
