#ifndef HG_SERVICES_AIR_RATING_H
#define HG_SERVICES_AIR_RATING_H

#include "hg/types.h"

/* Pure: a known Poor channel wins immediately. Good/Fair require CO2, PM2.5 and VOC;
 * otherwise the level is Unknown. Thresholds are in config.h. */
void hg_air_rate_indoor(hg_indoor_snapshot_t *indoor);

/* US AQI category 0..5 (Good .. Hazardous). */
int hg_air_us_aqi_category(int aqi);

#endif