#include "hg/app.h"

#include <Arduino.h>
#include <lvgl.h>

#include "hg/app/input_router.h"
#include "hg/config.h"
#include "hg/hal/display.h"
#include "hg/hal/led_strip.h"
#include "hg/hal/rtc.h"
#include "hg/hal/speaker.h"
#include "hg/services/alarm.h"
#include "hg/services/brightness.h"
#include "hg/services/display_mode.h"
#include "hg/services/indoor_air.h"
#include "hg/services/phone_bridge.h"
#include "hg/services/store.h"
#include "hg/services/time.h"
#include "hg/services/weather.h"
#include "hg/services/wifi.h"
#include "hg/types.h"
#include "hg/log.h"
#include "hg/ui/boot_overlay.h"
#include "hg/ui/clock_screen.h"
#include "hg/ui/navigator.h"
#include "hg/ui/overlays.h"
#include "hg/ui/theme.h"
#include "hg/ui/weather_screen.h"

static uint32_t last_tick_ms = 0;
static uint32_t last_boot_overlay_tick_ms = 0;

static void hg_app_push_clock(void) {
    hg_clock_snapshot_t snap;
    hg_time_get_snapshot(&snap.time);
    hg_wifi_get_snapshot(&snap.wifi);
    hg_alarm_get_snapshot(&snap.alarm);
    snap.display_active = hg_display_mode_get() != HG_DISPLAY_MODE_OFF;
    hg_clock_screen_update(&snap);
}

static void hg_app_push_weather(void) {
    hg_weather_snapshot_t weather;
    hg_indoor_snapshot_t indoor;
    hg_weather_get_snapshot(&weather);
    hg_indoor_air_get_snapshot(&indoor);
    hg_weather_screen_update(&weather, &indoor);
}

static void hg_app_sync_alarm_page(void) {
    hg_alarm_snapshot_t alarm;
    hg_alarm_get_snapshot(&alarm);
    if (alarm.ringing && hg_navigator_current() != HG_PAGE_CLOCK) {
        hg_navigator_show_page(HG_PAGE_CLOCK);
    }
}

void hg_app_setup(void) {
    hg_wifi_snapshot_t wifi_snap;
    hg_time_snapshot_t time_snap;

    hg_display_init();
    hg_theme_init();
    hg_navigator_init();
    hg_clock_screen_create(hg_navigator_page(HG_PAGE_CLOCK));
    hg_weather_screen_create(hg_navigator_page(HG_PAGE_WEATHER));

    hg_store_init();
    hg_wifi_init();
    hg_time_init();
    hg_weather_init();
    hg_indoor_air_init();
    hg_alarm_init();
    hg_brightness_init();
    hg_display_mode_init();
    hg_phone_bridge_init();

    hg_speaker_init();
    hg_led_strip_init();
    hg_rtc_init();

    hg_boot_overlay_create();
    hg_wifi_get_snapshot(&wifi_snap);
    hg_time_get_snapshot(&time_snap);
    /* Establish the correct theme before the overlay's first paint, or it flashes white. */
    hg_brightness_update();
    hg_display_mode_update();
    hg_theme_set_nightlight(hg_display_mode_get() != HG_DISPLAY_MODE_NORMAL);
    hg_boot_overlay_update(&wifi_snap, &time_snap, hg_wifi_get_target_ssid());
    hg_overlays_init();
    hg_input_router_init();

    last_tick_ms = 0;
    last_boot_overlay_tick_ms = 0;
}

void hg_app_loop(void) {
    /* Sample before lv_timer_handler(): slide-ready clears sliding inside it. */
    const bool sliding_at_entry = hg_navigator_is_sliding();
    HG_LOG_TASK_IF("lvgl", 5, lv_timer_handler());

    if (hg_boot_overlay_is_active()) {
        uint32_t now = millis();

        if (now - last_boot_overlay_tick_ms >= HG_BOOT_OVERLAY_TICK_MS) {
            hg_wifi_snapshot_t wifi_snap;
            hg_time_snapshot_t time_snap;

            last_boot_overlay_tick_ms = now;
            HG_LOG_TASK("wifi", hg_wifi_update());
            hg_wifi_get_snapshot(&wifi_snap);
            HG_LOG_TASK("time", hg_time_get_snapshot(&time_snap));
            HG_LOG_TASK("bright", hg_brightness_update());
            HG_LOG_TASK("disp", hg_display_mode_update());
            hg_theme_set_nightlight(hg_display_mode_get() != HG_DISPLAY_MODE_NORMAL);
            HG_LOG_TASK("boot", (hg_boot_overlay_update(&wifi_snap, &time_snap,
                                                        hg_wifi_get_target_ssid())));

            /* Push real values immediately so the clock never shows stale placeholder text. */
            if (!hg_boot_overlay_is_active()) {
                HG_LOG_TASK("clock", hg_app_push_clock());
                HG_LOG_TASK("wxui", hg_app_push_weather());
            }
        }

        delay(HG_LVGL_IDLE_DELAY_MS);
        return;
    }

    HG_LOG_TASK_IF("input", 2, hg_input_router_update());
    HG_LOG_TASK_IF("nav", 2, hg_navigator_update());

    if (sliding_at_entry || hg_navigator_is_sliding()) {
        if (!hg_navigator_is_sliding()) {
            delay(HG_LVGL_IDLE_DELAY_MS);
        }
        return;
    }

    uint32_t now = millis();
    if (now - last_tick_ms >= HG_UI_TICK_MS) {
        hg_wifi_snapshot_t wifi_snap;
        hg_time_snapshot_t time_snap;

        last_tick_ms = now;
        HG_LOG("app", "ui tick\n");
        HG_LOG_TASK("wifi", hg_wifi_update());
        hg_wifi_get_snapshot(&wifi_snap);
        HG_LOG_TASK("time", hg_time_get_snapshot(&time_snap));
        HG_LOG_TASK("boot", (hg_boot_overlay_update(&wifi_snap, &time_snap,
                                                    hg_wifi_get_target_ssid())));

        if (hg_boot_overlay_is_active()) {
            delay(HG_LVGL_IDLE_DELAY_MS);
            return;
        }

        HG_LOG_TASK("wxsvc", hg_weather_update());
        HG_LOG_TASK("indoor", hg_indoor_air_update());
        HG_LOG_TASK("alarm", hg_alarm_update());
        HG_LOG_TASK("bright", hg_brightness_update());
        HG_LOG_TASK("disp", hg_display_mode_update());
        hg_theme_set_nightlight(hg_display_mode_get() != HG_DISPLAY_MODE_NORMAL);
        HG_LOG_TASK("phone", hg_phone_bridge_update());
        HG_LOG_TASK("overlay", hg_overlays_update());
        HG_LOG_TASK("nav", hg_app_sync_alarm_page());
        HG_LOG_TASK("clock", hg_app_push_clock());
        HG_LOG_TASK("wxui", hg_app_push_weather());
    }

    delay(HG_LVGL_IDLE_DELAY_MS);
}
