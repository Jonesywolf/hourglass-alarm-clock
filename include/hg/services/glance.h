#ifndef HG_SERVICES_GLANCE_H
#define HG_SERVICES_GLANCE_H

#include <stdbool.h>
#include <stdint.h>

#include "hg/types.h"

/* Pure clock-face policy. Line priority: thunderstorm, poor indoor air, unhealthy outdoor AQI,
 * post-alarm briefing (HG_GLANCE_BRIEFING_MS after the alarm is stopped), gusts, heat, cold, UV.
 * Weather lines need valid weather younger than HG_WEATHER_EXPIRED_MS. */
void hg_glance_compute(const hg_weather_snapshot_t *weather, const hg_indoor_snapshot_t *indoor,
                       uint32_t now_ms, bool alarm_stopped, uint32_t alarm_stopped_ms,
                       hg_glance_snapshot_t *out);

#endif
