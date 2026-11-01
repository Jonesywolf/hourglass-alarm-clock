#include "hg/app.h"
#include "hg/app/simulator.h"
#include "hg/app/input_router.h"
#include "hg/app/runtime.h"
#include "hg/services/alarm.h"
#include "hg/services/display_mode.h"
#include "hg/services/effects.h"
#include "hg/services/glance.h"
#include "hg/services/store.h"
#include "hg/services/time.h"
#include "hg/ui/boot_overlay.h"
#include "hg/ui/navigator.h"
#include "hg/ui/overlays.h"

#include <emscripten.h>
#include <lvgl.h>
#include <stdio.h>

static uint32_t now_ms = 0;
static uint32_t next_loop = 0;
static bool started = false;

extern "C" uint32_t millis(void) { return now_ms; }
extern "C" uint32_t micros(void) { return now_ms * 1000U; }
extern "C" void delay(uint32_t ms) {
    if (!started) {
        now_ms += ms;
    } else {
        next_loop = now_ms + (ms ? ms : 1);
    }
}
uint32_t hg_sim_now(void) { return now_ms; }
void hg_sim_advance(uint32_t ms) {
    uint32_t remaining = ms;
    do {
        hg_sim_capture_state();
        if ((int32_t)(now_ms - next_loop) >= 0) {
            next_loop = now_ms + 1;
            hg_app_loop();
        }
        if (!remaining) break;
        ++now_ms;
        --remaining;
    } while (true);
    hg_sim_render();
}

extern "C" {
EMSCRIPTEN_KEEPALIVE void sim_start(int enabled, int hour, int minute, int volume, int sound) {
    hg_alarm_snapshot_t alarm = {(bool)enabled, false, hour, minute, false,
                                 (uint8_t)volume, (hg_alarm_sound_t)sound};
    hg_sim_seed_alarm(&alarm);
    hg_app_setup();
    started = true;
    hg_sim_advance(0);
}
EMSCRIPTEN_KEEPALIVE void sim_advance(int ms) { hg_sim_advance(ms); }
EMSCRIPTEN_KEEPALIVE int sim_input(int kind, int value) { return hg_sim_input(kind, value); }
EMSCRIPTEN_KEEPALIVE void sim_cancel_inputs(void) { hg_sim_cancel_inputs(); }
EMSCRIPTEN_KEEPALIVE void sim_time(int hour, int minute, int second, int valid) {
    hg_sim_set_time(hour, minute, second, valid);
}
EMSCRIPTEN_KEEPALIVE void sim_environment(int wifi, float lux) {
    hg_sim_set_environment(wifi, lux);
}
EMSCRIPTEN_KEEPALIVE void sim_weather(int status, int condition, float temp, float feels,
                                     float high, float low, float wind, float gust, float uv,
                                     int rain, int aqi, int air_status) {
    hg_sim_weather_t weather = {status, condition, temp, feels, high, low, wind, gust, uv,
                                rain, aqi, air_status};
    hg_sim_set_weather(&weather);
}
EMSCRIPTEN_KEEPALIVE void sim_indoor(int status, float temp, float humidity, int co2,
                                    float pm25, int voc) {
    hg_sim_set_indoor(status, temp, humidity, co2, pm25, voc);
}
EMSCRIPTEN_KEEPALIVE void sim_units(int units) { hg_sim_set_units(units); }
EMSCRIPTEN_KEEPALIVE void sim_fault(int value) { hg_sim_set_fault(value); }
EMSCRIPTEN_KEEPALIVE void sim_colors(void) {
    const uint32_t colors[] = {0xff0000, 0x00ff00, 0x0000ff, 0xffffff, 0x000000};
    for (int i = 0; i < 5; ++i) {
        lv_obj_t *patch = lv_obj_create(lv_layer_top());
        lv_obj_set_pos(patch, i * 64, 0);
        lv_obj_set_size(patch, 64, 240);
        lv_obj_set_style_bg_color(patch, lv_color_hex(colors[i]), 0);
        lv_obj_set_style_bg_opa(patch, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(patch, 0, 0);
        lv_obj_set_style_radius(patch, 0, 0);
        lv_obj_clear_flag(patch, LV_OBJ_FLAG_SCROLLABLE);
    }
    hg_sim_render();
}
EMSCRIPTEN_KEEPALIVE const char *sim_state(void) {
    static char json[1536];
    hg_alarm_snapshot_t alarm;
    hg_alarm_edit_snapshot_t edit;
    hg_volume_snapshot_t volume;
    hg_effects_snapshot_t effects;
    hg_store_status_t store;
    hg_runtime_diagnostics_t diag;
    hg_time_snapshot_t time;
    hg_alarm_get_snapshot(&alarm);
    hg_input_router_get_alarm_edit(&edit);
    hg_input_router_get_volume(&volume);
    hg_effects_get_snapshot(&effects);
    hg_store_get_status(&store);
    hg_runtime_get_diagnostics(&diag);
    hg_time_get_snapshot(&time);
    hg_network_snapshot_t network;
    hg_sensor_snapshot_t sensors;
    hg_glance_snapshot_t glance;
    hg_weather_view_t view;
    uint32_t stopped_ms = 0;
    hg_runtime_get_network(&network);
    hg_runtime_get_sensors(&sensors);
    bool stopped = hg_alarm_last_stopped(&stopped_ms);
    hg_glance_compute(&network.weather, &sensors.indoor, now_ms, stopped, stopped_ms, &glance);
    hg_input_router_get_weather_view(&view);
    snprintf(json, sizeof(json),
        "{\"now\":%u,\"frames\":%u,\"page\":%d,\"sliding\":%s,\"boot\":%s,"
        "\"displayMode\":%d,\"brightness\":%u,\"alarm\":{\"enabled\":%s,"
        "\"ringing\":%s,\"snoozed\":%s,\"hour\":%d,\"minute\":%d,\"volume\":%u,\"sound\":%d},"
        "\"edit\":{\"field\":%d,\"hour\":%d,\"minute\":%d,\"sound\":%d},"
        "\"volumeVisible\":%s,\"volumeAnimating\":%s,\"alarmOverlay\":%s,"
        "\"effects\":{\"phase\":%d,\"generation\":%u,\"audioAvailable\":false,\"ledAvailable\":false},"
        "\"store\":{\"pending\":%s,\"requested\":%u,\"saved\":%u,\"writes\":%u},"
        "\"input\":{\"encoderDown\":%s,\"touchDown\":%s,\"overflows\":%u,\"expired\":%u},"
        "\"time\":{\"hour\":%d,\"minute\":%d,\"second\":%d},"
        "\"weather\":{\"offset\":%u,\"glanceLine\":%d,\"showWeather\":%s,\"indoorLevel\":%d}}",
        now_ms, hg_sim_frames(), hg_navigator_current(), hg_navigator_is_sliding() ? "true" : "false",
        hg_boot_overlay_is_active() ? "true" : "false", hg_display_mode_get(), hg_sim_brightness(),
        alarm.enabled ? "true" : "false", alarm.ringing ? "true" : "false",
        alarm.snoozed ? "true" : "false", alarm.hour, alarm.minute, alarm.volume_pct, alarm.sound,
        edit.field, edit.hour, edit.minute, edit.sound, volume.visible ? "true" : "false",
        hg_overlays_volume_animating() ? "true" : "false",
        hg_overlays_alarm_visible() ? "true" : "false", effects.phase, effects.generation,
        store.pending ? "true" : "false", store.requested_generation, store.saved_generation,
        store.successful_saves, diag.encoder_pressed ? "true" : "false",
        diag.touch_pressed ? "true" : "false", diag.input_overflows, diag.expired_inputs,
        time.hour_24, time.minute, time.second, view.hourly_offset, glance.line,
        glance.show_weather ? "true" : "false", sensors.indoor.level);
    return json;
}
}

int main(void) { return 0; }
