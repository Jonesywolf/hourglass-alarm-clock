#ifndef HG_SERVICES_INDOOR_AIR_H
#define HG_SERVICES_INDOOR_AIR_H

#include "hg/types.h"

void hg_indoor_air_init(void);
void hg_indoor_air_update(void);
void hg_indoor_air_get_snapshot(hg_indoor_snapshot_t *out);

#endif
