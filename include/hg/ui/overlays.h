#ifndef HG_UI_OVERLAYS_H
#define HG_UI_OVERLAYS_H

#include "hg/types.h"

void hg_overlays_init(void);
void hg_overlays_update(void);
/* Shows the alarm screen while ringing, then briefly confirms the outcome
 * (snoozed / next alarm / alarm off). Call whenever the alarm snapshot may have changed. */
void hg_overlays_set_alarm(const hg_alarm_snapshot_t *alarm);
bool hg_overlays_alarm_visible(void);
void hg_overlays_set_volume(const hg_volume_snapshot_t *volume);
bool hg_overlays_volume_animating(void);

#ifdef HG_RUNTIME_TEST
#include <lvgl.h>
bool hg_overlays_test_volume(int *x, int *value);
lv_obj_t *hg_overlays_test_volume_root(void);
#endif

#endif
