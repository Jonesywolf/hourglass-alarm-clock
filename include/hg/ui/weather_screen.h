#ifndef HG_UI_WEATHER_SCREEN_H
#define HG_UI_WEATHER_SCREEN_H

#include <lvgl.h>

#include "hg/types.h"

void hg_weather_screen_create(lv_obj_t *parent);
/* Header (location + freshness), hero (icon, temperature, condition, feels-like, high/low),
 * an hourly strip of HG_WEATHER_HOURLY_VISIBLE columns starting at view.hourly_offset, and
 * wind / outdoor AQI / indoor air chips. */
void hg_weather_screen_update(const hg_weather_page_snapshot_t *snap);

#endif
