#ifndef HG_TYPES_H
#define HG_TYPES_H

#include <stdbool.h>
#include <stdint.h>

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
} hg_alarm_snapshot_t;

typedef struct {
    hg_time_snapshot_t time;
    hg_wifi_snapshot_t wifi;
    hg_alarm_snapshot_t alarm;
    bool display_active;
} hg_clock_snapshot_t;

typedef struct {
    bool valid;
    float temp_c;
    float humidity;
    float pm2_5;
    int aqi;
} hg_indoor_snapshot_t;

typedef struct {
    bool valid;
    float temp_c;
    int weather_code;
    int aqi;
} hg_weather_snapshot_t;

#endif
