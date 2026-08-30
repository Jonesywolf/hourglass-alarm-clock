#ifndef HG_SERVICES_WEATHER_H
#define HG_SERVICES_WEATHER_H

#include "hg/types.h"

void hg_weather_init(void);
void hg_weather_update(void);
void hg_weather_get_snapshot(hg_weather_snapshot_t *out);

#endif
