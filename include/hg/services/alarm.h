#ifndef HG_SERVICES_ALARM_H
#define HG_SERVICES_ALARM_H

#include "hg/types.h"

void hg_alarm_init(void);
void hg_alarm_update(void);
void hg_alarm_get_snapshot(hg_alarm_snapshot_t *out);
void hg_alarm_set_time(int hour, int minute);
void hg_alarm_set_enabled(bool enabled);
void hg_alarm_dismiss(void);
/* Rings again HG_ALARM_SNOOZE_MIN minutes from now; the saved time is untouched. No-op unless ringing. */
void hg_alarm_snooze(void);
/* False briefly after dismiss/snooze so a repeated press cannot disable tomorrow's alarm. */
bool hg_alarm_toggle_allowed(uint32_t now_ms);

#endif
