#ifndef HG_SERVICES_ALARM_H
#define HG_SERVICES_ALARM_H

#include "hg/types.h"

void hg_alarm_init(void);
void hg_alarm_update(void);
void hg_alarm_get_snapshot(hg_alarm_snapshot_t *out);
void hg_alarm_set_time(int hour, int minute);
void hg_alarm_set_enabled(bool enabled);

#endif
