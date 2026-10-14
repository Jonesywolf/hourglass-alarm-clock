#include "hg/app.h"

#include <Arduino.h>
#include <lvgl.h>

#include "hg/app/input_router.h"
#include "hg/app/runtime.h"
#include "hg/config.h"
#include "hg/hal/display.h"
#include "hg/hal/led_strip.h"
#include "hg/hal/rtc.h"
#include "hg/hal/speaker.h"
#include "hg/log.h"
#include "hg/services/alarm.h"
#include "hg/services/brightness.h"
#include "hg/services/display_mode.h"
#include "hg/services/effects.h"
#include "hg/services/store.h"
#include "hg/services/time.h"
#include "hg/services/wifi.h"
#include "hg/ui/boot_overlay.h"
#include "hg/ui/clock_screen.h"
#include "hg/ui/navigator.h"
#include "hg/ui/overlays.h"
#include "hg/ui/theme.h"
#include "hg/ui/weather_screen.h"

static uint32_t last_policy_ms = 0;
static uint32_t last_ui_ms = 0;
static uint32_t last_boot_ms = 0;
static uint32_t last_activity_ms = 0;
static bool runtime_ready = false;
static bool display_ready = false;
static bool fault_shown = false;
static bool ui_dirty = true;

#ifdef HG_RUNTIME_TEST
static bool hide_boot_overlay = false;

void hg_app_test_hide_boot_overlay(bool hide) {
    hide_boot_overlay = hide;
    if (hide) {
        hg_boot_overlay_destroy();
    }
}
#endif

static void push_screens(const hg_network_snapshot_t *network,
                         const hg_sensor_snapshot_t *sensors) {
    hg_clock_snapshot_t clock;
    hg_time_get_snapshot(&clock.time);
    clock.wifi = network->wifi;
    hg_alarm_get_snapshot(&clock.alarm);
    clock.display_active = hg_display_mode_get() != HG_DISPLAY_MODE_OFF;
    hg_clock_screen_update(&clock);
    hg_weather_screen_update(&network->weather, &sensors->indoor);
}

static void update_effects(uint32_t now) {
    hg_alarm_snapshot_t alarm;
    hg_alarm_get_snapshot(&alarm);
    /* Sunrise scheduling will supply its own phase when implemented. */
    hg_effects_set_alarm(false, alarm.ringing);
    hg_effects_update(now);
    hg_effects_snapshot_t effects;
    hg_effects_get_snapshot(&effects);
    hg_runtime_set_indoor_paused(effects.pause_indoor);
    hg_store_set_flash_allowed(!hg_navigator_is_sliding() && !effects.timing_sensitive &&
                               now - last_activity_ms >= HG_STORE_QUIET_MS);
    if (alarm.ringing && hg_navigator_current() != HG_PAGE_CLOCK) {
        hg_navigator_show_page(HG_PAGE_CLOCK);
        hg_store_set_flash_allowed(false);
    }
}

void hg_app_setup(void) {
    hg_log_init();
    display_ready = hg_display_init();
    if (!display_ready) {
        HG_ERROR("display", "Display initialization failed\n");
        return;
    }
    hg_theme_init();
    hg_navigator_init();
    hg_clock_screen_create(hg_navigator_page(HG_PAGE_CLOCK));
    hg_weather_screen_create(hg_navigator_page(HG_PAGE_WEATHER));
    hg_boot_overlay_create();
    hg_overlays_init();
    hg_input_router_init();
    hg_store_init();
    hg_rtc_init();
    hg_time_init();
    hg_alarm_init();
    hg_brightness_init();
    hg_display_mode_init();
    hg_effects_init();
    bool audio_available = hg_speaker_init();
    bool led_available = hg_led_strip_init();
    hg_effects_set_capabilities(led_available, audio_available);
    runtime_ready = hg_runtime_start();
    if (!runtime_ready) {
        hg_boot_overlay_show_fault(hg_runtime_fault());
        fault_shown = true;
        return;
    }

    hg_sensor_snapshot_t sensors;
    uint32_t started = millis();
    // Before the first paint only: allow the initial light conversion to set the theme.
    do {
        hg_runtime_get_sensors(&sensors);
        if (sensors.light.valid || sensors.light.status == HG_SAMPLE_ERROR) {
            break;
        }
        delay(1);
    } while (millis() - started < 200U);
    hg_brightness_update(&sensors.light);
    hg_display_mode_update();
    hg_theme_set_nightlight(hg_display_mode_get() != HG_DISPLAY_MODE_NORMAL);
    hg_network_snapshot_t network;
    hg_runtime_get_network(&network);
    hg_time_snapshot_t time;
    hg_time_get_snapshot(&time);
    hg_boot_overlay_update(&network.wifi, &time, hg_wifi_get_target_ssid());
    push_screens(&network, &sensors);
    last_policy_ms = millis() - HG_POLICY_TICK_MS;
    last_ui_ms = millis();
    last_boot_ms = millis();
    last_activity_ms = millis();
}

void hg_app_loop(void) {
    if (!display_ready) {
        hg_log_flush();
        delay(20);
        return;
    }
    hg_display_poll();
    const char *fault = hg_runtime_fault();
    if (!runtime_ready || fault) {
        if (!fault_shown) {
            hg_boot_overlay_show_fault(fault);
            fault_shown = true;
        }
        lv_timer_handler();
        if (!runtime_ready) {
            hg_log_flush();
        }
        delay(HG_LVGL_IDLE_DELAY_MS);
        return;
    }

    uint32_t now = millis();
    hg_sensor_snapshot_t sensors;
    hg_network_snapshot_t network;
    hg_runtime_get_sensors(&sensors);
    hg_runtime_get_network(&network);
    if (now - last_policy_ms >= HG_POLICY_TICK_MS) {
        last_policy_ms = now;
        hg_alarm_update();
        hg_brightness_update(&sensors.light);
        hg_display_mode_update();
        update_effects(now);
    }

    bool update_boot = true;
#ifdef HG_RUNTIME_TEST
    update_boot = !hide_boot_overlay;
#endif
    if (update_boot && now - last_boot_ms >= HG_BOOT_OVERLAY_TICK_MS) {
        last_boot_ms = now;
        hg_time_snapshot_t time;
        hg_time_get_snapshot(&time);
        bool was_active = hg_boot_overlay_is_active();
        hg_boot_overlay_update(&network.wifi, &time, hg_wifi_get_target_ssid());
        ui_dirty = ui_dirty || (was_active && !hg_boot_overlay_is_active());
    }
    bool navigation_allowed = !hg_boot_overlay_is_active();
    hg_runtime_set_navigation_allowed(navigation_allowed);
    if (hg_input_router_update(navigation_allowed)) {
        last_activity_ms = now;
        hg_store_set_flash_allowed(false);
    }
    hg_navigator_update();
    update_effects(now);

    if (now - last_ui_ms >= HG_UI_TICK_MS) {
        last_ui_ms = now;
        ui_dirty = true;
    }
    bool sliding = hg_navigator_is_sliding();
    if (!sliding) {
        hg_theme_set_nightlight(hg_display_mode_get() != HG_DISPLAY_MODE_NORMAL);
        hg_overlays_update();
        if (ui_dirty) {
            push_screens(&network, &sensors);
            ui_dirty = false;
        }
    } else {
        last_activity_ms = now;
        hg_store_set_flash_allowed(false);
    }

    hg_display_measure_frames(sliding);
    uint32_t started_us = micros();
    uint32_t wait_ms = lv_timer_handler();
    hg_runtime_record_ui(started_us, micros() - started_us, sliding);
    if (wait_ms > HG_LVGL_IDLE_DELAY_MS) {
        wait_ms = HG_LVGL_IDLE_DELAY_MS;
    }
    delay(sliding || wait_ms == 0 ? 1 : wait_ms);
}
