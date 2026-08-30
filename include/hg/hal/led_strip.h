#ifndef HG_HAL_LED_STRIP_H
#define HG_HAL_LED_STRIP_H

#include <stdbool.h>
#include <stdint.h>

bool hg_led_strip_init(void);
void hg_led_strip_set_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

#endif
