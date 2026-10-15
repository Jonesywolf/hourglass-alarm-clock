#ifndef HG_UI_OVERLAYS_H
#define HG_UI_OVERLAYS_H

#include "hg/types.h"

void hg_overlays_init(void);
void hg_overlays_update(void);
/* Shows the alarm screen while ringing, then briefly confirms the outcome
 * (snoozed / next alarm / alarm off). Call whenever the alarm snapshot may have changed. */
void hg_overlays_set_alarm(const hg_alarm_snapshot_t *alarm);
bool hg_overlays_alarm_visible(void);

#endif
