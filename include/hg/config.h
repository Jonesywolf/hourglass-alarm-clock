#ifndef HG_CONFIG_H
#define HG_CONFIG_H

#define HG_DISP_HOR_RES 320
#define HG_DISP_VER_RES 240
#define HG_DISP_DRAW_BUF_LINES 40

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
#define HG_LOG_DEBUG 0
#define HG_LOG_TOUCH_DEBUG 1

#define HG_ALARM_STORE_DEBOUNCE_MS 800U
#define HG_ALARM_SNOOZE_MIN 15
/* After a dismiss/snooze, an encoder press must not also toggle the alarm off. */
#define HG_ALARM_TOGGLE_LOCKOUT_MS 2000U
#define HG_ALARM_CONFIRM_MS 2500U
#define HG_ENCODER_HOLD_MS 1000U
#define HG_ALARM_EDIT_IDLE_MS 10000U
#define HG_ALARM_EDIT_BLINK_MS 500U
#define HG_ALARM_VOLUME_MIN_PCT 5
#define HG_ALARM_VOLUME_DEFAULT_PCT 75
#define HG_ALARM_VOLUME_STEP_PCT 5
#define HG_VOLUME_HIDE_MS 3000U
#define HG_VOLUME_SLIDE_MS 150U

/* Alarm audio. The clip in src/services/sound_alarm_clip.cpp must use HG_AUDIO_SAMPLE_RATE. */
#define HG_AUDIO_SAMPLE_RATE 16000U
#define HG_AUDIO_CHUNK_FRAMES 256U
#define HG_AUDIO_DMA_BUF_COUNT 6U
#define HG_AUDIO_WRITE_TIMEOUT_MS 100U
#define HG_AUDIO_RETRY_MS 1000U
#define HG_AUDIO_FADE_IN_MS 50U
#define HG_UI_CUE_VOLUME_PCT 2
#define HG_AUDIO_DRAIN_FRAMES ((HG_AUDIO_DMA_BUF_COUNT + 1U) * HG_AUDIO_CHUNK_FRAMES)
#ifdef HG_RUNTIME_TEST
/* Automated tests ring the alarm on real hardware; keep them quiet. */
#define HG_ALARM_VOLUME_START_PCT 1
#define HG_ALARM_VOLUME_MAX_PCT 2
#else
#define HG_ALARM_VOLUME_START_PCT 5
#define HG_ALARM_VOLUME_MAX_PCT 80
#endif
#define HG_ALARM_RAMP_MS 30000U

#define HG_INPUT_POLL_MS 5U
#define HG_INPUT_QUEUE_CAPACITY 16U
#define HG_INPUT_MAX_AGE_MS 250U
#define HG_POLICY_TICK_MS 20U
#define HG_LIGHT_SAMPLE_MS 250U
#define HG_INDOOR_SAMPLE_MS 1000U
#define HG_SENSOR_STALE_MS 3000U
#define HG_NETWORK_STALE_MS 5000U
#define HG_SENSOR_RETRY_MS 5000U
#define HG_I2C_TIMEOUT_MS 20U
#define HG_STORE_RETRY_MS 2000U
#define HG_STORE_QUIET_MS 500U
#define HG_FEEDBACK_DURATION_MS 150U
#define HG_FEEDBACK_INTERVAL_MS 80U
#define HG_CUE_FEEDBACK_DURATION_MS 450U

/* ESP32-S3 Touch V2: counts rise when touched. Idle <30k, press >=33k. */
#define HG_TOUCH_DEBOUNCE_MS 30U
#define HG_TOUCH_BASELINE_SAMPLES 16U
#define HG_TOUCH_THRESHOLD_DELTA 2000U

#define HG_PAGE_SLIDE_MS 300U
#define HG_WEATHER_IDLE_RETURN_MS (5U * 60U * 1000U)
#define HG_WEATHER_HOURLY_VISIBLE 5

/* Weather fetching (weather worker). Snapshots are metric; the UI converts per unit system. */
#define HG_DEFAULT_UNITS HG_UNITS_METRIC
#define HG_WEATHER_REFRESH_MS (15UL * 60UL * 1000UL)
#define HG_AIR_REFRESH_MS (30UL * 60UL * 1000UL)
#define HG_LOCATION_REFRESH_MS (24UL * 60UL * 60UL * 1000UL)
#define HG_WEATHER_RETRY_MIN_MS 30000UL
#define HG_WEATHER_RETRY_MAX_MS (10UL * 60UL * 1000UL)
#define HG_WEATHER_HTTP_TIMEOUT_MS 8000U
#define HG_WEATHER_STALE_MS (45UL * 60UL * 1000UL)
/* Beyond this the clock-face chip is hidden instead of shown muted. */
#define HG_WEATHER_EXPIRED_MS (3UL * 60UL * 60UL * 1000UL)
#define HG_WEATHER_RAIN_PROB_PCT 50

/* Clock-face glance: briefing window after the alarm is stopped, and outlier thresholds. */
#define HG_GLANCE_BRIEFING_MS (15UL * 60UL * 1000UL)
#define HG_GLANCE_GUST_KPH 50.0f
#define HG_GLANCE_OUTDOOR_AQI 101
#define HG_GLANCE_UV_INDEX 8.0f
#define HG_GLANCE_HEAT_C 35.0f
#define HG_GLANCE_COLD_C (-15.0f)

/* Indoor air rating: Fair at/above the first value, Poor at/above the second. */
#define HG_INDOOR_CO2_FAIR_PPM 1000
#define HG_INDOOR_CO2_POOR_PPM 1500
#define HG_INDOOR_PM25_FAIR 12.1f
#define HG_INDOOR_PM25_POOR 35.5f
#define HG_INDOOR_VOC_FAIR 150
#define HG_INDOOR_VOC_POOR 250

/* Lux hysteresis band: <= DARK enters low-light, >= BRIGHT returns to normal, in between holds. */
#define HG_DISPLAY_DARK_LUX 5.0f
#define HG_DISPLAY_BRIGHT_LUX 50.0f

#define HG_DISPLAY_RED_TO_OFF_MIN 60
#define HG_DISPLAY_INPUT_WAKE_MS 60000U
#define HG_DISPLAY_PRE_ALARM_MIN 60

#endif
