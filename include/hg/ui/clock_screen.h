#ifndef HG_UI_CLOCK_SCREEN_H
#define HG_UI_CLOCK_SCREEN_H

#include <lvgl.h>

#include "hg/types.h"

void hg_clock_screen_create(lv_obj_t *parent);
void hg_clock_screen_update(const hg_clock_snapshot_t *snap);
void hg_clock_screen_update_alarm(const hg_alarm_snapshot_t *alarm);

#endif
