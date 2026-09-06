#ifndef HG_SERVICES_BRIGHTNESS_H
#define HG_SERVICES_BRIGHTNESS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HG_LIGHT_DARK = 0,
    HG_LIGHT_NEUTRAL,
    HG_LIGHT_BRIGHT
} hg_light_level_t;

void hg_brightness_init(void);
void hg_brightness_update(void);
hg_light_level_t hg_brightness_get_light_level(void);
uint8_t hg_brightness_get_percent(void);

#endif
