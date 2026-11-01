#include "hg/ui/weather_format.h"

#include <math.h>
#include <stdio.h>

#include "hg/ui/theme.h"

#define MINUS "\xE2\x88\x92"
#define DEGREE "\xC2\xB0"
#define DOT " \xC2\xB7 "

int hg_wx_temp_value(float temp_c, hg_unit_system_t units) {
    float value = units == HG_UNITS_IMPERIAL ? temp_c * 9.0f / 5.0f + 32.0f : temp_c;
    return (int)lroundf(value);
}

void hg_wx_format_temp(char *out, size_t size, float temp_c, hg_unit_system_t units) {
    if (isnan(temp_c)) {
        snprintf(out, size, "--" DEGREE);
        return;
    }
    int value = hg_wx_temp_value(temp_c, units);
    snprintf(out, size, "%s%d" DEGREE, value < 0 ? MINUS : "", value < 0 ? -value : value);
}

void hg_wx_format_speed(char *out, size_t size, float kph, hg_unit_system_t units) {
    if (isnan(kph)) {
        snprintf(out, size, "--");
    } else if (units == HG_UNITS_IMPERIAL) {
        snprintf(out, size, "%ld mph", lroundf(kph * 0.621371f));
    } else {
        snprintf(out, size, "%ld km/h", lroundf(kph));
    }
}

void hg_wx_format_hour(char *out, size_t size, int hour_24) {
    int hour12 = hour_24 % 12;
    snprintf(out, size, "%d%s", hour12 == 0 ? 12 : hour12, hour_24 >= 12 ? "pm" : "am");
}

const char *hg_wx_condition_name(hg_weather_condition_t condition) {
    static const char *const names[HG_WX_COUNT] = {
        "Unknown", "Clear", "Partly cloudy", "Cloudy", "Fog", "Drizzle", "Rain", "Heavy rain",
        "Freezing rain", "Snow", "Heavy snow", "Thunderstorms", "Hail",
    };
    return (unsigned)condition < HG_WX_COUNT ? names[condition] : names[0];
}

const char *hg_wx_aqi_name(uint8_t category) {
    static const char *const names[] = {
        "Good", "Moderate", "Unhealthy for some", "Unhealthy", "Very unhealthy", "Hazardous",
    };
    return names[category < 6 ? category : 5];
}

const char *hg_wx_air_level_name(hg_air_level_t level) {
    switch (level) {
        case HG_AIR_GOOD:
            return "Good";
        case HG_AIR_FAIR:
            return "Fair";
        case HG_AIR_POOR:
            return "Poor";
        default:
            return "--";
    }
}

lv_color_t hg_wx_aqi_color(uint8_t category) {
    /* EPA category palette, lightened to read on black. */
    static const uint32_t colors[] = {0x4ADE80, 0xFACC15, 0xFB923C, 0xF87171, 0xC084FC, 0xBE7B8E};
    return hg_theme_signal(lv_color_hex(colors[category < 6 ? category : 5]));
}

lv_color_t hg_wx_air_level_color(hg_air_level_t level) {
    switch (level) {
        case HG_AIR_GOOD:
            return hg_wx_aqi_color(0);
        case HG_AIR_FAIR:
            return hg_wx_aqi_color(1);
        case HG_AIR_POOR:
            return hg_wx_aqi_color(3);
        default:
            return hg_theme_muted();
    }
}

lv_color_t hg_wx_rain_color(void) {
    return hg_theme_signal(lv_color_hex(0x60A5FA));
}

static void format_briefing(char *out, size_t size, const hg_glance_snapshot_t *glance,
                            hg_unit_system_t units) {
    char high[16], low[16];
    hg_wx_format_temp(high, sizeof(high), glance->high_c, units);
    hg_wx_format_temp(low, sizeof(low), glance->low_c, units);
    if (glance->rain_start_hour >= 0) {
        char hour[8];
        hg_wx_format_hour(hour, sizeof(hour), glance->rain_start_hour);
        snprintf(out, size, "%s / %s" DOT "Rain from %s", high, low, hour);
    } else {
        snprintf(out, size, "%s / %s" DOT "%s today", high, low,
                 hg_wx_condition_name(glance->day_condition));
    }
}

void hg_wx_format_glance_line(char *out, size_t size, const hg_glance_snapshot_t *glance,
                              hg_unit_system_t units) {
    char value[16];
    out[0] = '\0';
    if (!glance) {
        return;
    }
    switch (glance->line) {
        case HG_GLANCE_LINE_THUNDER:
            snprintf(out, size, glance->value > 0.5f ? "Thunderstorm now" : "Storms likely today");
            break;
        case HG_GLANCE_LINE_INDOOR_POOR:
            if (glance->indoor_cause == HG_AIR_CAUSE_CO2) {
                snprintf(out, size, "Stuffy inside" DOT "CO2 %ld ppm", lroundf(glance->value));
            } else if (glance->indoor_cause == HG_AIR_CAUSE_VOC) {
                snprintf(out, size, "Indoor air poor" DOT "VOC %ld", lroundf(glance->value));
            } else {
                snprintf(out, size, "Indoor air poor" DOT "PM2.5 %ld", lroundf(glance->value));
            }
            break;
        case HG_GLANCE_LINE_OUTDOOR_AQI:
            snprintf(out, size, "Outdoor air %s" DOT "AQI %ld",
                     hg_wx_aqi_name(glance->aqi_category), lroundf(glance->value));
            break;
        case HG_GLANCE_LINE_GUSTS:
            hg_wx_format_speed(value, sizeof(value), glance->value, units);
            snprintf(out, size, "Gusts up to %s", value);
            break;
        case HG_GLANCE_LINE_HEAT:
            hg_wx_format_temp(value, sizeof(value), glance->value, units);
            snprintf(out, size, "Hot today" DOT "up to %s", value);
            break;
        case HG_GLANCE_LINE_COLD:
            hg_wx_format_temp(value, sizeof(value), glance->value, units);
            snprintf(out, size, "Bitter cold" DOT "down to %s", value);
            break;
        case HG_GLANCE_LINE_UV:
            snprintf(out, size, "High UV today" DOT "%ld", lroundf(glance->value));
            break;
        case HG_GLANCE_LINE_BRIEFING:
            format_briefing(out, size, glance, units);
            break;
        default:
            break;
    }
}
