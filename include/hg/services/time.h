#ifndef HG_SERVICES_TIME_H
#define HG_SERVICES_TIME_H

#include "hg/types.h"

void hg_time_init(void);
void hg_time_get_snapshot(hg_time_snapshot_t *out);

#endif
