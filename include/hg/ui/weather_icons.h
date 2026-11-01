#ifndef HG_UI_WEATHER_ICONS_H
#define HG_UI_WEATHER_ICONS_H

#include <lvgl.h>
#include <stdbool.h>

#include "hg/types.h"

typedef enum {
    HG_WX_ICON_SMALL = 0, /* 40 px: clock glance, hourly strip */
    HG_WX_ICON_LARGE      /* 80 px: weather page hero */
} hg_wx_icon_size_t;

/* Points an lv_img at the condition's art. Normal theme draws full color (dimmed when muted);
 * nightlight draws the luminance mask recolored with the theme so no blue/yellow light leaks. */
void hg_wx_icon_apply(lv_obj_t *img, hg_weather_condition_t condition, bool is_day,
                      hg_wx_icon_size_t size, bool muted);

/* Center the visible artwork and temperature glyphs, excluding transparent/font padding. */
void hg_wx_icon_align_temperature(lv_obj_t *img, lv_obj_t *temperature, lv_coord_t center_y);

#endif
