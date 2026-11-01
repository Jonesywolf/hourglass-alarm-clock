#ifndef HG_UI_WEATHER_FORMAT_H
#define HG_UI_WEATHER_FORMAT_H

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

#include "hg/types.h"

/* Display formatting shared by the clock glance and the weather page. Snapshots stay metric;
 * conversion happens only here. Negative values use U+2212, present in every Inter font. */
int hg_wx_temp_value(float temp_c, hg_unit_system_t units);
/* "21°" (no unit letter; the unit system is global). */
void hg_wx_format_temp(char *out, size_t size, float temp_c, hg_unit_system_t units);
/* "32 km/h" / "20 mph". */
void hg_wx_format_speed(char *out, size_t size, float kph, hg_unit_system_t units);
/* "Now" for offset 0 is the caller's choice; this returns "3pm" / "12am". */
void hg_wx_format_hour(char *out, size_t size, int hour_24);
const char *hg_wx_condition_name(hg_weather_condition_t condition);
const char *hg_wx_aqi_name(uint8_t category);
const char *hg_wx_air_level_name(hg_air_level_t level);
/* Category colors; nightlight folds them into the red theme. */
lv_color_t hg_wx_aqi_color(uint8_t category);
lv_color_t hg_wx_air_level_color(hg_air_level_t level);
lv_color_t hg_wx_rain_color(void);
/* Clock-face context line; empty string for HG_GLANCE_LINE_NONE. */
void hg_wx_format_glance_line(char *out, size_t size, const hg_glance_snapshot_t *glance,
                              hg_unit_system_t units);

#endif
