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
#include "hg/services/indoor_air.h"
#include "hg/services/phone_bridge.h"
#include "hg/services/store.h"
#include "hg/services/time.h"
#include "hg/services/weather.h"
#include "hg/services/wifi.h"
#include "hg/types.h"
#include "hg/ui/boot_overlay.h"
#include "hg/ui/clock_screen.h"
#include "hg/ui/navigator.h"
#include "hg/ui/overlays.h"
#include "hg/ui/theme.h"

static uint32_t last_tick_ms = 0;
static uint32_t last_boot_overlay_tick_ms = 0;

static void hg_app_push_clock(void) {
    hg_clock_snapshot_t snap;
    hg_time_get_snapshot(&snap.time);
    hg_wifi_get_snapshot(&snap.wifi);
    hg_alarm_get_snapshot(&snap.alarm);
    hg_clock_screen_update(&snap);
}

void hg_app_setup(void) {
    hg_wifi_snapshot_t wifi_snap;
    hg_time_snapshot_t time_snap;

    hg_display_init();
    hg_theme_init();
    hg_clock_screen_create(lv_scr_act());

    hg_store_init();
    hg_wifi_init();
    hg_time_init();
    hg_weather_init();
    hg_indoor_air_init();
    hg_alarm_init();
    hg_brightness_init();
    hg_phone_bridge_init();

    hg_speaker_init();
    hg_led_strip_init();
    hg_rtc_init();

    hg_navigator_init();
    hg_boot_overlay_create();
    hg_wifi_get_snapshot(&wifi_snap);
    hg_time_get_snapshot(&time_snap);
    hg_boot_overlay_update(&wifi_snap, &time_snap, hg_wifi_get_target_ssid());
    hg_overlays_init();
    hg_input_router_init();

    last_tick_ms = 0;
    last_boot_overlay_tick_ms = 0;
}

void hg_app_loop(void) {
    lv_timer_handler();

    if (hg_boot_overlay_is_active()) {
        uint32_t now = millis();

        if (now - last_boot_overlay_tick_ms >= HG_BOOT_OVERLAY_TICK_MS) {
            hg_wifi_snapshot_t wifi_snap;
            hg_time_snapshot_t time_snap;

            last_boot_overlay_tick_ms = now;
            hg_wifi_update();
            hg_wifi_get_snapshot(&wifi_snap);
            hg_time_get_snapshot(&time_snap);
            hg_boot_overlay_update(&wifi_snap, &time_snap, hg_wifi_get_target_ssid());
        }

        delay(HG_LVGL_IDLE_DELAY_MS);
        return;
    }

    hg_input_router_update();

    uint32_t now = millis();
    if (now - last_tick_ms >= HG_UI_TICK_MS) {
        hg_wifi_snapshot_t wifi_snap;
        hg_time_snapshot_t time_snap;

        last_tick_ms = now;
        hg_wifi_update();
        hg_wifi_get_snapshot(&wifi_snap);
        hg_time_get_snapshot(&time_snap);
        hg_boot_overlay_update(&wifi_snap, &time_snap, hg_wifi_get_target_ssid());

        if (hg_boot_overlay_is_active()) {
            delay(HG_LVGL_IDLE_DELAY_MS);
            return;
        }

        hg_weather_update();
        hg_indoor_air_update();
        hg_alarm_update();
        hg_brightness_update();
        hg_phone_bridge_update();
        hg_overlays_update();
        hg_app_push_clock();
    }

    delay(HG_LVGL_IDLE_DELAY_MS);
}
