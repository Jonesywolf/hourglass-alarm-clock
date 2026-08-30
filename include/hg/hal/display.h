#ifndef HG_HAL_DISPLAY_H
#define HG_HAL_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

bool hg_display_init(void);
void hg_display_set_brightness(uint8_t percent);

#endif
