#ifndef HG_CONFIG_H
#define HG_CONFIG_H

#define HG_DISP_HOR_RES 320
#define HG_DISP_VER_RES 240
#define HG_DISP_DRAW_BUF_LINES 10

#define HG_NTP_SERVER "pool.ntp.org"
#define HG_GMT_OFFSET_SEC (-28800L)
#define HG_DAYLIGHT_OFFSET_SEC 3600

#define HG_WIFI_RETRY_INITIAL_MS 1000U
#define HG_WIFI_RETRY_MAX_MS 30000U
#define HG_WIFI_CONNECT_ATTEMPT_MS 12000U
#define HG_BOOT_OVERLAY_TICK_MS 750U
#define HG_UI_TICK_MS 1000U
#define HG_LVGL_IDLE_DELAY_MS 5

/* 1 = tagged Serial logs via HG_LOG / HG_LOG_TASK. 0 on device; Serial stalls frames. */
#define HG_LOG_DEBUG 1

#define HG_ALARM_STORE_DEBOUNCE_MS 800U

#define HG_TIME_QUERY_TIMEOUT_MS 10U

/* ESP32-S3 Touch V2: counts rise when touched. Idle <30k, press >=33k. */
#define HG_TOUCH_DEBOUNCE_MS 30U
#define HG_TOUCH_BASELINE_SAMPLES 16U
#define HG_TOUCH_THRESHOLD_DELTA 2000U

#define HG_PAGE_SLIDE_MS 300U
#define HG_WEATHER_IDLE_RETURN_MS (5U * 60U * 1000U)

/* Lux hysteresis band: <= DARK enters low-light, >= BRIGHT returns to normal, in between holds. */
#define HG_DISPLAY_DARK_LUX 5.0f
#define HG_DISPLAY_BRIGHT_LUX 50.0f

#define HG_DISPLAY_RED_TO_OFF_MIN 60
#define HG_DISPLAY_PRE_ALARM_MIN 60

#endif
