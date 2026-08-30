#ifndef HG_HAL_RTC_H
#define HG_HAL_RTC_H

#include <stdbool.h>

#include "hg/types.h"

bool hg_rtc_init(void);
bool hg_rtc_get_time(hg_time_snapshot_t *out);
bool hg_rtc_set_time(const hg_time_snapshot_t *in);

#endif
