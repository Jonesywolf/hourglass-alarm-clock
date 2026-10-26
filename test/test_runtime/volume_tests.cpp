#include <Arduino.h>
#include <climits>
#include <cstring>
#include <initializer_list>
#include <lvgl.h>
#include <unity.h>

#include "hg/app.h"
#include "hg/app/input_router.h"
#include "hg/app/runtime.h"
#include "hg/config.h"
#include "hg/hal/display.h"
#include "hg/services/alarm.h"
#include "hg/services/display_mode.h"
#include "hg/services/effects.h"
#include "hg/services/sound.h"
#include "hg/services/store.h"
#include "hg/ui/navigator.h"
#include "hg/ui/fonts.h"
#include "hg/ui/overlays.h"
#include "hg/ui/theme.h"

static bool active;
static uint32_t now_ms;

static hg_alarm_snapshot_t alarm(void) {
    hg_alarm_snapshot_t value;
    hg_alarm_get_snapshot(&value);
    return value;
}

static hg_volume_snapshot_t volume(void) {
    hg_volume_snapshot_t value;
    hg_input_router_get_volume(&value);
    return value;
}

static void turn(int32_t delta, uint32_t elapsed = 1, bool allowed = true) {
    now_ms += elapsed;
    hg_input_event_t event = {now_ms, hg_runtime_input_generation(), false, false, delta};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    hg_input_router_test_update(allowed, now_ms);
}

static void set_volume(int pct) {
    hg_alarm_adjust_volume(INT32_MIN);
    hg_alarm_adjust_volume((pct - HG_ALARM_VOLUME_MIN_PCT) / HG_ALARM_VOLUME_STEP_PCT);
}

static void begin(void) {
    active = true;
    hg_store_test_writer(0, false);
    hg_store_set_flash_allowed(false);
    hg_app_test_hide_boot_overlay(true);
    hg_alarm_set_enabled(false);
    hg_alarm_set_time(9, 17);
    hg_navigator_show_page(HG_PAGE_CLOCK);
    uint32_t started = millis();
    while ((hg_navigator_is_sliding() || hg_overlays_alarm_visible()) &&
           millis() - started < HG_ALARM_CONFIRM_MS + 2000U) {
        hg_app_loop();
    }
    TEST_ASSERT_FALSE(hg_navigator_is_sliding());
    TEST_ASSERT_FALSE(hg_overlays_alarm_visible());
    hg_runtime_test_manual_input(true);
    hg_input_event_t discarded;
    while (hg_runtime_take_input(&discarded)) {}
    hg_input_router_init();
    hg_runtime_set_navigation_allowed(true);
    hg_display_mode_init();
    set_volume(HG_ALARM_VOLUME_DEFAULT_PCT);
    now_ms = millis();
    turn(0);
}

void volume_test_cleanup(void) {
    if (!active) {
        return;
    }
    active = false;
    hg_input_router_init();
    hg_volume_snapshot_t hidden = {false, true, 0};
    hg_overlays_set_volume(&hidden);
    hg_alarm_set_enabled(false);
    set_volume(HG_ALARM_VOLUME_DEFAULT_PCT);
    hg_display_mode_init();
    hg_theme_set_nightlight(false);
    hg_runtime_test_manual_input(true);
}

void test_volume_steps_clamps_and_timeout(void) {
    begin();
    turn(1);
    TEST_ASSERT_EQUAL(80, alarm().volume_pct);
    TEST_ASSERT_TRUE(volume().visible);
    TEST_ASSERT_EQUAL(80, volume().volume_pct);
    turn(-2);
    TEST_ASSERT_EQUAL(70, alarm().volume_pct);
    turn(INT32_MAX);
    TEST_ASSERT_EQUAL(100, alarm().volume_pct);
    turn(INT32_MIN);
    TEST_ASSERT_EQUAL(5, alarm().volume_pct);
    turn(0, HG_VOLUME_HIDE_MS - 1);
    TEST_ASSERT_TRUE(volume().visible);
    turn(-1);
    turn(0, HG_VOLUME_HIDE_MS - 1);
    TEST_ASSERT_TRUE(volume().visible);
    turn(0);
    TEST_ASSERT_FALSE(volume().visible);
    TEST_ASSERT_FALSE(volume().dismiss_immediately);
    TEST_ASSERT_EQUAL(9, alarm().hour);
    TEST_ASSERT_EQUAL(17, alarm().minute);
    TEST_ASSERT_FALSE(alarm().enabled);

    now_ms = UINT32_MAX - 100;
    turn(1);
    turn(0, HG_VOLUME_HIDE_MS - 1);
    TEST_ASSERT_TRUE(volume().visible);
    turn(0);
    TEST_ASSERT_FALSE(volume().visible);
}

void test_volume_context_and_snooze_isolation(void) {
    begin();
    turn(1, 1, false);
    TEST_ASSERT_EQUAL(75, alarm().volume_pct);
    hg_input_router_set_volume_allowed(false);
    turn(1);
    TEST_ASSERT_EQUAL(75, alarm().volume_pct);
    hg_input_router_set_volume_allowed(true);
    hg_input_event_t stale = {now_ms - HG_INPUT_MAX_AGE_MS - 1,
                              hg_runtime_input_generation(), false, false, 1};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&stale));
    hg_input_router_test_update(true, now_ms);
    TEST_ASSERT_EQUAL(75, alarm().volume_pct);
    hg_input_event_t old = {now_ms, hg_runtime_input_generation() - 1, false, false, 1};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&old));
    hg_input_router_test_update(true, now_ms);
    TEST_ASSERT_EQUAL(75, alarm().volume_pct);
    turn(1);
    hg_navigator_show_page(HG_PAGE_WEATHER);
    turn(1);
    TEST_ASSERT_EQUAL(80, alarm().volume_pct);
    TEST_ASSERT_FALSE(volume().visible);
    TEST_ASSERT_TRUE(volume().dismiss_immediately);
    hg_navigator_show_page(HG_PAGE_CLOCK);
    uint32_t started = millis();
    while (hg_navigator_is_sliding() && millis() - started < 2000U) {
        hg_app_loop();
    }
    now_ms = millis();
    turn(0);
    hg_alarm_set_enabled(true);
    hg_time_snapshot_t due = {true, 9, 17, 0};
    hg_alarm_test_check_time(&due);
    TEST_ASSERT_TRUE(alarm().ringing);
    turn(1);
    TEST_ASSERT_EQUAL(80, alarm().volume_pct);
    TEST_ASSERT_FALSE(volume().visible);
    hg_alarm_snooze();
    TEST_ASSERT_TRUE(alarm().snoozed);
    int snooze_hour = alarm().hour, snooze_minute = alarm().minute;
    hg_input_router_set_volume_allowed(true);
    turn(-1);
    TEST_ASSERT_EQUAL(75, alarm().volume_pct);
    TEST_ASSERT_TRUE(volume().visible);
    TEST_ASSERT_TRUE(alarm().snoozed);
    TEST_ASSERT_EQUAL(snooze_hour, alarm().hour);
    TEST_ASSERT_EQUAL(snooze_minute, alarm().minute);
    delay(HG_ALARM_STORE_DEBOUNCE_MS + 10);
    hg_alarm_update();
    hg_alarm_snapshot_t saved;
    hg_store_test_get_desired(&saved);
    TEST_ASSERT_EQUAL(9, saved.hour);
    TEST_ASSERT_EQUAL(17, saved.minute);
    TEST_ASSERT_EQUAL(75, saved.volume_pct);
    TEST_ASSERT_FALSE(saved.snoozed);
}

void test_volume_record_compatibility_and_coalescing(void) {
    begin();
    uint8_t bytes[] = {0x31, 0x41, 0x47, 0x48, 1, 6, 42, 0};
    hg_alarm_snapshot_t decoded = {};
    TEST_ASSERT_TRUE(hg_store_test_decode_alarm(bytes, sizeof(bytes), &decoded));
    TEST_ASSERT_EQUAL(HG_ALARM_VOLUME_DEFAULT_PCT, decoded.volume_pct);
    TEST_ASSERT_EQUAL(6, decoded.hour);
    TEST_ASSERT_EQUAL(42, decoded.minute);
    TEST_ASSERT_TRUE(decoded.enabled);
    for (uint8_t pct : {5, 75, 100}) {
        bytes[7] = pct;
        TEST_ASSERT_TRUE(hg_store_test_decode_alarm(bytes, sizeof(bytes), &decoded));
        TEST_ASSERT_EQUAL(pct, decoded.volume_pct);
    }
    bytes[7] = 101;
    TEST_ASSERT_TRUE(hg_store_test_decode_alarm(bytes, sizeof(bytes), &decoded));
    TEST_ASSERT_EQUAL(HG_ALARM_VOLUME_DEFAULT_PCT, decoded.volume_pct);
    TEST_ASSERT_EQUAL(42, decoded.minute);
    TEST_ASSERT_FALSE(hg_store_test_decode_alarm(bytes, sizeof(bytes) - 1, &decoded));
    bytes[5] = 24;
    TEST_ASSERT_FALSE(hg_store_test_decode_alarm(bytes, sizeof(bytes), &decoded));

    hg_alarm_set_enabled(false);
    hg_store_status_t before, after;
    hg_store_get_status(&before);
    turn(1);
    turn(1);
    turn(-1);
    hg_store_get_status(&after);
    TEST_ASSERT_EQUAL_UINT32(before.requested_generation, after.requested_generation);
    delay(HG_ALARM_STORE_DEBOUNCE_MS + 10);
    hg_alarm_update();
    hg_store_get_status(&after);
    TEST_ASSERT_EQUAL_UINT32(before.requested_generation + 1, after.requested_generation);
    hg_alarm_snapshot_t desired;
    hg_store_test_get_desired(&desired);
    TEST_ASSERT_EQUAL(80, desired.volume_pct);
    hg_store_request_save_alarm(&desired);
    hg_store_get_status(&after);
    TEST_ASSERT_EQUAL_UINT32(before.requested_generation + 1, after.requested_generation);
}

void test_volume_scaled_pcm_and_unchanged_cues(void) {
    const uint32_t rate = HG_AUDIO_SAMPLE_RATE;
    int16_t reference[256], scaled[256];
    hg_sound_player_t full, quiet;
    for (uint8_t pct : {5, 75, 100}) {
        TEST_ASSERT_TRUE(hg_sound_player_start(&full, HG_SOUND_ALARM, rate, 100));
        TEST_ASSERT_TRUE(hg_sound_player_start(&quiet, HG_SOUND_ALARM, rate, pct));
        for (uint32_t elapsed : {0U, rate, rate * 31U}) {
            full.elapsed = quiet.elapsed = elapsed;
            full.position = quiet.position = 0;
            TEST_ASSERT_EQUAL(256, hg_sound_player_render(&full, reference, 256));
            TEST_ASSERT_EQUAL(256, hg_sound_player_render(&quiet, scaled, 256));
            for (unsigned i = 0; i < 256; ++i) {
                TEST_ASSERT_INT_WITHIN(1, (int)(reference[i] * pct / 100.0f), scaled[i]);
                TEST_ASSERT_LESS_OR_EQUAL(32767 * HG_ALARM_VOLUME_MAX_PCT / 100,
                                         abs((int)scaled[i]));
            }
        }
    }
    TEST_ASSERT_TRUE(hg_sound_player_start(&full, HG_SOUND_ALARM_SAVE, rate, 100));
    TEST_ASSERT_TRUE(hg_sound_player_start(&quiet, HG_SOUND_ALARM_SAVE, rate, 5));
    hg_sound_player_render(&full, reference, 256);
    hg_sound_player_render(&quiet, scaled, 256);
    TEST_ASSERT_EQUAL_MEMORY(reference, scaled, sizeof(reference));
}

void test_volume_panel_animation_and_cancellation(void) {
    begin();
    hg_volume_snapshot_t hidden = {};
    hidden.dismiss_immediately = true;
    hg_overlays_set_volume(&hidden);
    turn(-1);
    hg_volume_snapshot_t snap = volume();
    lv_timer_t *refresh = _lv_disp_get_refr_timer(lv_disp_get_default());
    lv_timer_reset(refresh);
    hg_overlays_set_volume(&snap);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(refresh->period, lv_tick_elaps(refresh->last_run));
    int x, value;
    TEST_ASSERT_TRUE(hg_overlays_test_volume(&x, &value));
    TEST_ASSERT_EQUAL(70, value);
    uint32_t started = millis();
    while (millis() - started < HG_VOLUME_SLIDE_MS + 30) {
        hg_display_poll();
        lv_timer_handler();
        delay(5);
    }
    TEST_ASSERT_TRUE(hg_overlays_test_volume(&x, &value));
    TEST_ASSERT_EQUAL(HG_DISP_HOR_RES - 28, x);
    lv_obj_t *root = hg_overlays_test_volume_root();
    TEST_ASSERT_EQUAL(160, lv_obj_get_height(root));
    TEST_ASSERT_EQUAL(HG_DISP_HOR_RES, x + lv_obj_get_width(root));
    TEST_ASSERT_EQUAL(2, lv_obj_get_child_cnt(root));
    TEST_ASSERT_FALSE(lv_obj_has_flag(root, LV_OBJ_FLAG_CLICKABLE));
    lv_obj_t *bar = lv_obj_get_child(root, 0);
    TEST_ASSERT_GREATER_THAN(lv_obj_get_width(bar), lv_obj_get_height(bar));
    TEST_ASSERT_EQUAL(0, lv_bar_get_min_value(bar));
    TEST_ASSERT_EQUAL(100, lv_bar_get_max_value(bar));
    lv_obj_t *icon = lv_obj_get_child(root, 1);
    TEST_ASSERT_EQUAL_STRING(HG_ICON_VOLUME_HIGH, lv_label_get_text(icon));
    for (uint8_t pct : {5, 50, 55, 100, 50}) {
        snap.volume_pct = pct;
        hg_overlays_set_volume(&snap);
        TEST_ASSERT_EQUAL_STRING(pct > 50 ? HG_ICON_VOLUME_HIGH : HG_ICON_VOLUME_LOW,
                                  lv_label_get_text(icon));
    }
    snap.visible = false;
    hg_overlays_set_volume(&snap);
    delay(50);
    hg_display_poll();
    lv_timer_handler();
    snap.visible = true;
    snap.volume_pct = 65;
    hg_overlays_set_volume(&snap);
    hg_display_poll();
    hg_theme_set_nightlight(true);
    started = millis();
    while (millis() - started < HG_VOLUME_SLIDE_MS + 30) {
        hg_overlays_set_volume(&snap);
        lv_timer_handler();
        delay(5);
    }
    TEST_ASSERT_TRUE(hg_overlays_test_volume(&x, &value));
    TEST_ASSERT_EQUAL(HG_DISP_HOR_RES - 28, x);
    TEST_ASSERT_EQUAL(65, value);
    TEST_ASSERT_EQUAL_STRING(HG_ICON_VOLUME_HIGH, lv_label_get_text(icon));
    TEST_ASSERT_EQUAL_UINT32(lv_color_to32(hg_theme_fg()),
        lv_color_to32(lv_obj_get_style_bg_color(bar, LV_PART_INDICATOR)));
    snap.visible = false;
    hg_overlays_set_volume(&snap);
    started = millis();
    while (millis() - started < HG_VOLUME_SLIDE_MS + 30) {
        hg_display_poll();
        lv_timer_handler();
        delay(5);
    }
    TEST_ASSERT_FALSE(hg_overlays_test_volume(&x, &value));
    TEST_ASSERT_TRUE(lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN));
    TEST_ASSERT_EQUAL(HG_DISP_HOR_RES, x);
    snap.visible = true;
    hg_overlays_set_volume(&snap);
    snap.visible = false;
    snap.dismiss_immediately = true;
    hg_overlays_set_volume(&snap);
    TEST_ASSERT_FALSE(hg_overlays_test_volume(&x, &value));
    TEST_ASSERT_EQUAL(HG_DISP_HOR_RES, x);
    TEST_ASSERT_FALSE(hg_overlays_alarm_visible());
}

void test_volume_immediate_app_handoff_and_editing(void) {
    begin();
    hg_input_event_t event = {millis(), hg_runtime_input_generation(), false, false, -1};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    hg_app_loop();
    int x, value;
    TEST_ASSERT_TRUE(hg_overlays_test_volume(&x, &value));
    TEST_ASSERT_EQUAL(70, value);
    TEST_ASSERT_FALSE(alarm().enabled);
    now_ms = millis();
    hg_input_event_t press = {now_ms, hg_runtime_input_generation(), false, true, 0};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&press));
    hg_input_router_test_update(true, now_ms);
    turn(0, HG_ENCODER_HOLD_MS);
    hg_alarm_edit_snapshot_t edit;
    hg_input_router_get_alarm_edit(&edit);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, edit.field);
    TEST_ASSERT_FALSE(volume().visible);
    TEST_ASSERT_TRUE(volume().dismiss_immediately);
    turn(1);
    hg_input_router_get_alarm_edit(&edit);
    TEST_ASSERT_EQUAL(18, edit.minute);
    TEST_ASSERT_EQUAL(70, alarm().volume_pct);
    hg_effects_snapshot_t effects;
    hg_effects_get_snapshot(&effects);
    TEST_ASSERT_NOT_EQUAL(HG_FEEDBACK_NAVIGATE, effects.feedback);
}
