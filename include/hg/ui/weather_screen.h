#ifndef HG_UI_WEATHER_SCREEN_H
#define HG_UI_WEATHER_SCREEN_H

#include <lvgl.h>

#include "hg/types.h"

void hg_weather_screen_create(lv_obj_t *parent);
void hg_weather_screen_update(const hg_weather_snapshot_t *weather,
                              const hg_indoor_snapshot_t *indoor);

#endif
