#ifndef HG_TYPES_H
#define HG_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HG_SAMPLE_WAITING = 0,
    HG_SAMPLE_READY,
    HG_SAMPLE_STALE,
    HG_SAMPLE_PAUSED,
    HG_SAMPLE_UNAVAILABLE,
    HG_SAMPLE_ERROR
} hg_sample_status_t;

typedef struct {
    bool valid;
    float lux;
    uint32_t sampled_ms;
    hg_sample_status_t status;
} hg_light_snapshot_t;

typedef struct {
    uint32_t captured_ms;
    uint32_t generation;
    bool tapped;
    bool encoder_pressed;
    int32_t encoder_delta;
    bool encoder_released;
    bool touch_released;
    uint32_t button_sequence;
} hg_input_event_t;

typedef struct {
    uint32_t captured_ms;
    uint32_t generation;
    uint32_t button_sequence;
    bool encoder_down;
    bool touch_down;
} hg_input_state_t;

typedef enum {
    HG_ALARM_SOUND_MALLETS = 0,
    HG_ALARM_SOUND_WOODEN,
    HG_ALARM_SOUND_PLUCKED,
    HG_ALARM_SOUND_COUNT
} hg_alarm_sound_t;

typedef enum {
    HG_ALARM_EDIT_NONE = 0,
    HG_ALARM_EDIT_MINUTE,
    HG_ALARM_EDIT_HOUR,
    HG_ALARM_EDIT_SOUND
} hg_alarm_edit_field_t;

typedef struct {
    hg_alarm_edit_field_t field;
    int hour;
    int minute;
    hg_alarm_sound_t sound;
} hg_alarm_edit_snapshot_t;

typedef struct {
    bool visible;
    bool dismiss_immediately;
    uint8_t volume_pct;
} hg_volume_snapshot_t;

typedef enum {
    HG_PAGE_CLOCK = 0,
    HG_PAGE_WEATHER,
    HG_PAGE_SETTINGS,
    HG_PAGE_COUNT
} hg_page_id_t;

typedef enum {
    HG_WIFI_DISCONNECTED = 0,
    HG_WIFI_CONNECTING,
    HG_WIFI_CONNECTED
} hg_wifi_state_t;

typedef enum {
    HG_WIFI_BARS_NONE = 0,
    HG_WIFI_BARS_1,
    HG_WIFI_BARS_2,
    HG_WIFI_BARS_3,
    HG_WIFI_BARS_4
} hg_wifi_bars_t;

typedef struct {
    bool valid;
    int hour_24;
    int minute;
    int second;
} hg_time_snapshot_t;

typedef struct {
    hg_wifi_state_t state;
    int rssi;
    hg_wifi_bars_t bars;
} hg_wifi_snapshot_t;

typedef struct {
    bool enabled;
    bool ringing;
    int hour;
    int minute;
    /* While snoozed, hour/minute are the temporary ring time; the saved time is restored on dismiss. */
    bool snoozed;
    uint8_t volume_pct;
    hg_alarm_sound_t sound;
} hg_alarm_snapshot_t;

typedef enum {
    HG_UNITS_METRIC = 0,
    HG_UNITS_IMPERIAL,
    HG_UNITS_COUNT
} hg_unit_system_t;

typedef struct {
    hg_unit_system_t units;
} hg_settings_t;

/* Overall air rating. Indoor uses the worst of CO2 / PM2.5 / VOC; there is no standard indoor AQI. */
typedef enum {
    HG_AIR_UNKNOWN = 0,
    HG_AIR_GOOD,
    HG_AIR_FAIR,
    HG_AIR_POOR
} hg_air_level_t;

typedef enum {
    HG_AIR_CAUSE_NONE = 0,
    HG_AIR_CAUSE_CO2,
    HG_AIR_CAUSE_PM2_5,
    HG_AIR_CAUSE_VOC
} hg_air_cause_t;

/* Sensor values are NaN (floats) or 0 (indices/ppm) when that channel is not reported. */
typedef struct {
    bool valid;
    float temp_c;
    float humidity;
    float pm1_0;
    float pm2_5;
    float pm4_0;
    float pm10;
    uint16_t co2_ppm;
    uint16_t voc_index;
    uint16_t nox_index;
    hg_air_level_t level;
    hg_air_cause_t cause;
    uint32_t sampled_ms;
    hg_sample_status_t status;
} hg_indoor_snapshot_t;

typedef enum {
    HG_WX_UNKNOWN = 0,
    HG_WX_CLEAR,
    HG_WX_PARTLY_CLOUDY,
    HG_WX_CLOUDY,
    HG_WX_FOG,
    HG_WX_DRIZZLE,
    HG_WX_RAIN,
    HG_WX_HEAVY_RAIN,
    HG_WX_FREEZING_RAIN,
    HG_WX_SNOW,
    HG_WX_HEAVY_SNOW,
    HG_WX_THUNDERSTORM,
    HG_WX_HAIL,
    HG_WX_COUNT
} hg_weather_condition_t;

#define HG_WEATHER_HOURS 24
#define HG_LOCATION_NAME_LEN 24

typedef struct {
    uint8_t hour_24;
    uint8_t condition; /* hg_weather_condition_t */
    bool is_day;
    uint8_t precip_prob_pct;
    float temp_c;
} hg_weather_hour_t;

typedef struct {
    bool valid;
    hg_sample_status_t status;
    uint32_t sampled_ms;
    /* Local time of the forecast fetch, for "Updated" text; -1 when unknown. */
    int16_t updated_minute_of_day;

    float temp_c;
    float feels_like_c;
    hg_weather_condition_t condition;
    bool is_day;
    uint8_t humidity_pct;
    float wind_kph;
    float gust_kph;
    uint16_t wind_dir_deg;

    float high_c;
    float low_c;
    hg_weather_condition_t day_condition;
    uint8_t day_precip_prob_pct;
    float day_precip_mm;
    float uv_max;
    float day_gust_max_kph;
    /* First remaining hour today (0-23) with precip probability above the rain threshold, or -1. */
    int8_t rain_start_hour;
    int8_t rain_end_hour;

    uint8_t hourly_count;
    hg_weather_hour_t hourly[HG_WEATHER_HOURS];

    bool air_valid;
    hg_sample_status_t air_status;
    uint16_t us_aqi;
    /* US AQI category 0..5: Good, Moderate, Unhealthy for sensitive groups, Unhealthy,
     * Very unhealthy, Hazardous. */
    uint8_t aqi_category;
    float air_pm2_5;

    char location[HG_LOCATION_NAME_LEN];
} hg_weather_snapshot_t;

typedef enum {
    HG_GLANCE_LINE_NONE = 0,
    HG_GLANCE_LINE_THUNDER,
    HG_GLANCE_LINE_INDOOR_POOR,
    HG_GLANCE_LINE_OUTDOOR_AQI,
    HG_GLANCE_LINE_GUSTS,
    HG_GLANCE_LINE_HEAT,
    HG_GLANCE_LINE_COLD,
    HG_GLANCE_LINE_UV,
    HG_GLANCE_LINE_BRIEFING
} hg_glance_line_t;

/* Clock-face "at a glance" content. Values are metric; the UI formats them per unit system. */
typedef struct {
    bool show_weather;
    bool muted;
    hg_weather_condition_t condition;
    bool is_day;
    float temp_c;
    hg_glance_line_t line;
    /* Line payload: briefing uses high/low/precip/rain_start_hour; outliers use value (+ cause).
     * Thunder: value 1 = storming now, 0 = expected today. Outdoor AQI: value + aqi_category. */
    float high_c;
    float low_c;
    uint8_t precip_prob_pct;
    int8_t rain_start_hour;
    hg_weather_condition_t day_condition;
    float value;
    hg_air_cause_t indoor_cause;
    uint8_t aqi_category;
} hg_glance_snapshot_t;

typedef struct {
    uint8_t hourly_offset;
} hg_weather_view_t;

typedef struct {
    hg_time_snapshot_t time;
    hg_wifi_snapshot_t wifi;
    hg_alarm_snapshot_t alarm;
    bool display_active;
    hg_alarm_edit_snapshot_t alarm_edit;
    hg_glance_snapshot_t glance;
    hg_unit_system_t units;
} hg_clock_snapshot_t;

typedef struct {
    hg_weather_snapshot_t weather;
    hg_indoor_snapshot_t indoor;
    hg_weather_view_t view;
    hg_unit_system_t units;
} hg_weather_page_snapshot_t;

#endif
