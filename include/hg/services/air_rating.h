#ifndef HG_SERVICES_AIR_RATING_H
#define HG_SERVICES_AIR_RATING_H

#include "hg/types.h"

/* Pure: sets indoor->level/cause from the worst of CO2, PM2.5 and VOC index (thresholds in config.h).
 * Unknown when the sample is invalid or reports none of those channels. */
void hg_air_rate_indoor(hg_indoor_snapshot_t *indoor);

/* US AQI category 0..5 (Good .. Hazardous). */
int hg_air_us_aqi_category(int aqi);

#endif