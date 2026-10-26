#ifndef HG_HAL_DISPLAY_H
#define HG_HAL_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include "hg/diagnostics.h"

bool hg_display_init(void);
void hg_display_poll(void);
/* On/off backlight control: zero is off, nonzero is on (no PWM dimming). */
void hg_display_set_brightness(uint8_t percent);
void hg_display_measure_frames(bool active);
void hg_display_get_frame_timing(hg_timing_stats_t *out);
void hg_display_reset_frame_timing(void);

typedef struct {
    uint32_t max_frame_transfer_us;
    uint32_t max_frame_pixels;
} hg_display_transfer_timing_t;

void hg_display_get_transfer_timing(hg_display_transfer_timing_t *out);

#endif
