#ifndef HG_SERVICES_BRIGHTNESS_H
#define HG_SERVICES_BRIGHTNESS_H

#include <stdbool.h>
#include <stdint.h>

void hg_brightness_init(void);
void hg_brightness_update(void);
void hg_brightness_set_nightlight(bool enabled);
uint8_t hg_brightness_get_percent(void);

#endif
