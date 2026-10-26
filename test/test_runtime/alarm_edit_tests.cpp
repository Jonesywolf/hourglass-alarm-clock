#include <Arduino.h>
#include <cstring>
#include <climits>
#include <unity.h>

#include "hg/app.h"
#include "hg/app/input_router.h"
#include "hg/app/runtime.h"
#include "hg/config.h"
#include "hg/hal/display.h"
#include "hg/hal/pins.h"
#include "hg/services/alarm.h"
#include "hg/services/brightness.h"
#include "hg/services/display_mode.h"
#include "hg/services/effects.h"
#include "hg/services/sound.h"
#include "hg/services/store.h"
#include "hg/services/time.h"
#include "hg/ui/clock_screen.h"
#include "hg/ui/navigator.h"
#include "hg/ui/theme.h"
#include "hg/ui/fonts.h"

static uint32_t now_ms;
static bool active = false;
static hg_effects_snapshot_t feedback(void);
static void reset_feedback(void);

static hg_alarm_edit_snapshot_t editor(void) {
    hg_alarm_edit_snapshot_t value;
    hg_input_router_get_alarm_edit(&value);
    return value;
}

static hg_alarm_snapshot_t alarm(void) {
    hg_alarm_snapshot_t value;
    hg_alarm_get_snapshot(&value);
    return value;
}

static void step(uint32_t elapsed, bool pressed = false, bool released = false,
                 int32_t delta = 0, bool tapped = false, bool touch_released = false,
                 bool allowed = true) {
    now_ms += elapsed;
    hg_input_event_t event = {now_ms, hg_runtime_input_generation(), tapped, pressed,
                              delta, released, touch_released};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    hg_input_router_test_update(allowed, now_ms);
}

static void short_press(void) {
    step(1, true);
    step(50, false, true);
}

static void hold(void) {
    step(1, true);
    step(HG_ENCODER_HOLD_MS);
}

static void begin(bool enabled = false, int hour = 23, int minute = 59) {
    active = true;
    hg_input_router_init();
    hg_app_test_hide_boot_overlay(true);
    hg_navigator_show_page(HG_PAGE_CLOCK);
    uint32_t started = millis();
    while (hg_navigator_is_sliding() && millis() - started < 2000U) {
        hg_app_loop();
    }
    TEST_ASSERT_FALSE(hg_navigator_is_sliding());
    hg_runtime_test_manual_input(true);
    hg_input_event_t discarded;
    while (hg_runtime_take_input(&discarded)) {}
    hg_input_router_init();
    hg_runtime_set_navigation_allowed(true);
    hg_alarm_set_enabled(false);
    hg_alarm_set_time(hour, minute);
    hg_alarm_set_enabled(enabled);
    hg_display_mode_init();
    now_ms = millis() + HG_ALARM_TOGGLE_LOCKOUT_MS + 1;
    step(0);
}

void test_alarm_editor_fields_and_commit(void) {
    begin();
    step(1, false, false, 20);
    TEST_ASSERT_EQUAL(59, alarm().minute);
    hg_store_status_t before, after;
    hg_store_get_status(&before);
    step(1, true);
    step(HG_ENCODER_HOLD_MS - 1);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    TEST_ASSERT_FALSE(alarm().enabled);
    step(1);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);
    step(HG_ENCODER_HOLD_MS);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);
    step(1, false, true);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);

    step(1, false, false, 1);
    TEST_ASSERT_EQUAL(0, editor().hour);
    TEST_ASSERT_EQUAL(0, editor().minute);
    step(1, false, false, -2);
    TEST_ASSERT_EQUAL(23, editor().hour);
    TEST_ASSERT_EQUAL(58, editor().minute);
    short_press();
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_HOUR, editor().field);
    step(1, false, false, 2);
    TEST_ASSERT_EQUAL(1, editor().hour);
    TEST_ASSERT_EQUAL(58, editor().minute);
    step(1, false, false, -26);
    TEST_ASSERT_EQUAL(23, editor().hour);
    short_press();
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_SOUND, editor().field);
    short_press();
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);
    step(1, false, false, 7);
    TEST_ASSERT_EQUAL(0, editor().hour);
    TEST_ASSERT_EQUAL(5, editor().minute);
    TEST_ASSERT_EQUAL(23, alarm().hour);
    TEST_ASSERT_EQUAL(59, alarm().minute);
    hg_store_get_status(&after);
    TEST_ASSERT_EQUAL_UINT32(before.requested_generation, after.requested_generation);

    hold();
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    TEST_ASSERT_EQUAL(0, alarm().hour);
    TEST_ASSERT_EQUAL(5, alarm().minute);
    TEST_ASSERT_FALSE(alarm().enabled);
    step(1, false, true);
    TEST_ASSERT_FALSE(alarm().enabled);
    delay(HG_ALARM_STORE_DEBOUNCE_MS + 10);
    hg_alarm_update();
    hg_store_get_status(&after);
    TEST_ASSERT_EQUAL_UINT32(before.requested_generation + 1, after.requested_generation);
}

void test_alarm_editor_touch_and_idle_save(void) {
    begin(true, 10, 10);
    hold();
    step(1, false, true);
    step(1, false, false, 3);
    uint32_t changed = now_ms;
    step(5000, false, false, 0, true);
    TEST_ASSERT_EQUAL(HG_PAGE_CLOCK, hg_navigator_current());
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);
    TEST_ASSERT_TRUE(alarm().enabled);
    step(1, false, false, 0, false, true);
    step(changed + HG_ALARM_EDIT_IDLE_MS - 1 - now_ms);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);
    step(1);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    TEST_ASSERT_EQUAL(13, alarm().minute);
    TEST_ASSERT_TRUE(alarm().enabled);

    hold();
    step(1, false, true);
    step(HG_ALARM_EDIT_IDLE_MS - 1, false, false, -1);
    step(HG_ALARM_EDIT_IDLE_MS - 1);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);
    short_press();
    step(HG_ALARM_EDIT_IDLE_MS - 1);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_HOUR, editor().field);
    step(1);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    TEST_ASSERT_EQUAL(12, alarm().minute);
}

void test_alarm_editor_suppression_and_matching_commit(void) {
    begin(true);
    hold();
    step(1, false, true);
    hg_time_snapshot_t time;
    hg_time_get_snapshot(&time);
    TEST_ASSERT_TRUE(time.valid);
    hg_time_snapshot_t due = {true, 23, 59, 0};
    hg_alarm_test_check_time(&due);
    TEST_ASSERT_FALSE(alarm().ringing);
    int delta = time.hour_24 * 60 + time.minute - (23 * 60 + 59);
    step(1, false, false, delta);
    hold();
    step(1, false, true);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    hg_alarm_update();
    TEST_ASSERT_FALSE(alarm().ringing);
    hg_alarm_test_check_time(&time);
    TEST_ASSERT_FALSE(alarm().ringing);
    due = time;
    due.minute = (due.minute + 1) % 60;
    hg_alarm_test_check_time(&due);
    TEST_ASSERT_FALSE(alarm().ringing);
    hg_alarm_test_check_time(&time);
    TEST_ASSERT_TRUE(alarm().ringing);
    hg_alarm_set_enabled(false);
}

void test_alarm_editor_page_and_generation_isolation(void) {
    begin();
    hg_navigator_show_page(HG_PAGE_WEATHER);
    short_press();
    hold();
    step(1, false, true, 100);
    TEST_ASSERT_FALSE(alarm().enabled);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    TEST_ASSERT_EQUAL(59, alarm().minute);

    begin();
    step(1, true, false, 0, false, false, false);
    step(HG_ENCODER_HOLD_MS);
    step(1, false, true);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    TEST_ASSERT_FALSE(alarm().enabled);

    step(1, true);
    hg_runtime_set_navigation_allowed(false);
    hg_runtime_set_navigation_allowed(true);
    step(HG_ENCODER_HOLD_MS);
    step(1, false, true);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    TEST_ASSERT_FALSE(alarm().enabled);
    hold();
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);
    step(1, false, true);
}

void test_alarm_editor_lost_release_and_wraparound(void) {
    begin();
    step(1, true);
    hg_input_event_t filler = {now_ms, hg_runtime_input_generation(), false, false, 0};
    for (unsigned i = 0; i < HG_INPUT_QUEUE_CAPACITY; ++i) {
        TEST_ASSERT_TRUE(hg_runtime_test_input(&filler));
    }
    hg_input_event_t release = {now_ms, hg_runtime_input_generation(), false, false, 0, true};
    TEST_ASSERT_FALSE(hg_runtime_test_input(&release));
    hg_input_router_test_update(true, now_ms);
    step(HG_ENCODER_HOLD_MS);
    TEST_ASSERT_FALSE(alarm().enabled);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    short_press();
    TEST_ASSERT_TRUE(alarm().enabled);

    now_ms = UINT32_MAX - HG_ENCODER_HOLD_MS / 2;
    step(0, true);
    step(HG_ENCODER_HOLD_MS - 1);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    step(1);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);
    step(1, false, true);
    now_ms = UINT32_MAX - HG_ALARM_EDIT_IDLE_MS / 2;
    step(0, false, false, 1);
    step(HG_ALARM_EDIT_IDLE_MS - 1);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);
    step(1);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);

    step(1, true);
    now_ms += HG_INPUT_MAX_AGE_MS + 1;
    hg_input_router_test_update(true, now_ms);
    step(HG_ENCODER_HOLD_MS);
    step(1, false, true);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    TEST_ASSERT_TRUE(alarm().enabled);
}

static void set_lux(float lux) {
    hg_light_snapshot_t light = {true, lux, millis(), HG_SAMPLE_READY};
    hg_brightness_update(&light);
}

static void enter_off(void) {
    hg_alarm_set_enabled(false);
    set_lux(1.0f);
    hg_display_mode_init();
    hg_display_mode_update(now_ms);
    now_ms += (uint32_t)HG_DISPLAY_RED_TO_OFF_MIN * 60000U;
    hg_display_mode_update(now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_OFF, hg_display_mode_get());
    TEST_ASSERT_EQUAL(LOW, digitalRead(HG_PIN_TFT_BL));
    set_lux(1.0f);
    TEST_ASSERT_EQUAL(LOW, digitalRead(HG_PIN_TFT_BL));
}

void test_nighttime_wake_gestures(void) {
    begin();
    enter_off();
    hold();
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    TEST_ASSERT_EQUAL(HIGH, digitalRead(HG_PIN_TFT_BL));
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    step(1, false, true);
    TEST_ASSERT_FALSE(alarm().enabled);
    hold();
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);
    step(1, false, true);
    hold();
    step(1, false, true);

    enter_off();
    int volume_before_wake = alarm().volume_pct;
    step(1, false, false, 10);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    TEST_ASSERT_EQUAL(HIGH, digitalRead(HG_PIN_TFT_BL));
    TEST_ASSERT_EQUAL(59, alarm().minute);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    TEST_ASSERT_EQUAL(volume_before_wake, alarm().volume_pct);
    hg_volume_snapshot_t volume;
    hg_input_router_get_volume(&volume);
    TEST_ASSERT_FALSE(volume.visible);

    enter_off();
    step(1, false, false, 0, true);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    TEST_ASSERT_EQUAL(HIGH, digitalRead(HG_PIN_TFT_BL));
    TEST_ASSERT_EQUAL(HG_PAGE_CLOCK, hg_navigator_current());
    step(1, false, false, 0, false, true);
    step(1, false, false, 0, true);
    TEST_ASSERT_EQUAL(HG_PAGE_WEATHER, hg_navigator_current());
}

void test_nighttime_timeouts_and_alarm_guard(void) {
    begin();
    set_lux(5.0f);
    hg_display_mode_update(now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    set_lux(25.0f);
    now_ms += (uint32_t)HG_DISPLAY_RED_TO_OFF_MIN * 60000U - 1;
    hg_display_mode_update(now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    hg_display_mode_update(++now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_OFF, hg_display_mode_get());
    TEST_ASSERT_EQUAL(LOW, digitalRead(HG_PIN_TFT_BL));
    TEST_ASSERT_TRUE(hg_display_mode_note_activity(now_ms));
    TEST_ASSERT_EQUAL(HIGH, digitalRead(HG_PIN_TFT_BL));
    now_ms += HG_DISPLAY_INPUT_WAKE_MS - 1;
    hg_display_mode_update(now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    TEST_ASSERT_FALSE(hg_display_mode_note_activity(now_ms));
    now_ms += HG_DISPLAY_INPUT_WAKE_MS - 1;
    hg_display_mode_update(now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    hg_display_mode_update(++now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_OFF, hg_display_mode_get());
    hg_display_mode_note_activity(now_ms);
    set_lux(50.0f);
    hg_display_mode_update(++now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_NORMAL, hg_display_mode_get());
    set_lux(1.0f);
    hg_display_mode_update(++now_ms);
    now_ms += HG_DISPLAY_INPUT_WAKE_MS;
    hg_display_mode_update(now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());

    hg_time_snapshot_t time;
    hg_time_get_snapshot(&time);
    TEST_ASSERT_TRUE(time.valid);
    hg_alarm_set_time(time.hour_24, time.minute);
    hg_alarm_set_enabled(true);
    now_ms += (uint32_t)HG_DISPLAY_RED_TO_OFF_MIN * 60000U;
    hg_display_mode_update(now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    hg_alarm_update();
    TEST_ASSERT_TRUE(alarm().ringing);
    hg_display_mode_update(++now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    hg_alarm_snooze();
    now_ms += HG_DISPLAY_INPUT_WAKE_MS;
    hg_display_mode_update(now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    hg_alarm_set_enabled(false);

    now_ms = UINT32_MAX - 10;
    hg_display_mode_update(now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_OFF, hg_display_mode_get());
    TEST_ASSERT_TRUE(hg_display_mode_note_activity(now_ms));
    now_ms += HG_DISPLAY_INPUT_WAKE_MS - 1;
    hg_display_mode_update(now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    hg_display_mode_update(++now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_OFF, hg_display_mode_get());
    TEST_ASSERT_EQUAL(LOW, digitalRead(HG_PIN_TFT_BL));

    set_lux(50.0f);
    TEST_ASSERT_EQUAL(LOW, digitalRead(HG_PIN_TFT_BL));
    hg_display_mode_update(++now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_NORMAL, hg_display_mode_get());
    TEST_ASSERT_EQUAL(HIGH, digitalRead(HG_PIN_TFT_BL));

    enter_off();
    hg_time_get_snapshot(&time);
    hg_alarm_set_time(time.hour_24, time.minute);
    hg_alarm_set_enabled(true);
    hg_display_mode_update(++now_ms);
    TEST_ASSERT_EQUAL(HG_DISPLAY_MODE_RED, hg_display_mode_get());
    TEST_ASSERT_EQUAL(HIGH, digitalRead(HG_PIN_TFT_BL));
}

void test_alarm_editor_flashing_presentation(void) {
    begin();
    hg_clock_snapshot_t snap = {};
    snap.display_active = true;
    snap.alarm = alarm();
    snap.alarm_edit = {HG_ALARM_EDIT_MINUTE, 0, 5};
    hg_clock_screen_update(&snap);
    lv_obj_t *label = hg_clock_screen_test_alarm_label();
    TEST_ASSERT_FALSE(lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN));
    TEST_ASSERT_EQUAL_STRING("12:05am", lv_label_get_text(label));
    lv_obj_update_layout(label);
    lv_coord_t width = lv_obj_get_width(label);
    uint32_t started = millis();
    while (millis() - started < HG_ALARM_EDIT_BLINK_MS + 20U) {
        hg_display_poll();
        lv_timer_handler();
        delay(1);
    }
    const char *text = lv_label_get_text(label);
    TEST_ASSERT_NOT_NULL(strstr(text, "12:#"));
    TEST_ASSERT_NOT_NULL(strstr(text, " 05#am"));
    lv_obj_update_layout(label);
    TEST_ASSERT_EQUAL(width, lv_obj_get_width(label));
    snap.alarm_edit = {HG_ALARM_EDIT_HOUR, 12, 5};
    hg_clock_screen_update(&snap);
    TEST_ASSERT_EQUAL_STRING("12:05pm", lv_label_get_text(label));
    started = millis();
    while (millis() - started < HG_ALARM_EDIT_BLINK_MS + 20U) {
        hg_display_poll();
        lv_timer_handler();
        delay(1);
    }
    text = lv_label_get_text(label);
    TEST_ASSERT_EQUAL_CHAR('#', text[0]);
    TEST_ASSERT_NOT_NULL(strstr(text, " 12#:05pm"));
    snap.alarm_edit.field = HG_ALARM_EDIT_NONE;
    hg_clock_screen_update(&snap);
    TEST_ASSERT_TRUE(lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN));
}

void test_alarm_sound_editor_and_presentation(void) {
    begin();
    hg_alarm_commit_edit(23, 59, HG_ALARM_SOUND_MALLETS);
    hold();
    step(1, false, true);
    short_press();
    short_press();
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_SOUND, editor().field);
    reset_feedback();
    step(1, false, false, -1);
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_PLUCKED, editor().sound);
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_MALLETS, alarm().sound);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, feedback().feedback);
    step(1, false, false, INT32_MAX);
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_MALLETS, editor().sound);
    step(1, false, false, INT32_MIN);
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_WOODEN, editor().sound);
    TEST_ASSERT_EQUAL(23, editor().hour);
    TEST_ASSERT_EQUAL(59, editor().minute);
    hg_alarm_snapshot_t value = alarm();
    uint8_t volume = value.volume_pct;
    step(1, false, false, 1);
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_PLUCKED, editor().sound);
    TEST_ASSERT_EQUAL(volume, alarm().volume_pct);
    hg_clock_snapshot_t snap = {};
    snap.display_active = true;
    snap.alarm = value;
    snap.alarm_edit = editor();
    hg_clock_screen_update(&snap);
    lv_obj_t *label = hg_clock_screen_test_sound_label();
    lv_obj_t *icon = hg_clock_screen_test_sound_icon();
    TEST_ASSERT_EQUAL_STRING(HG_ICON_MUSIC_NOTE, lv_label_get_text(icon));
    TEST_ASSERT_FALSE(lv_obj_has_flag(icon, LV_OBJ_FLAG_HIDDEN));
    TEST_ASSERT_FALSE(lv_obj_has_flag(icon, LV_OBJ_FLAG_CLICKABLE));
    lv_font_glyph_dsc_t glyph;
    TEST_ASSERT_TRUE(lv_font_get_glyph_dsc(HG_FONT_SOUND_ICON, &glyph, 0xE405, 0));
    TEST_ASSERT_FALSE(lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN));
    TEST_ASSERT_EQUAL_STRING("Plucked dawn", lv_label_get_text(label));
    lv_obj_update_layout(label);
    lv_area_t sound_area, time_area;
    lv_obj_get_coords(label, &sound_area);
    lv_area_t icon_area;
    lv_obj_get_coords(icon, &icon_area);
    TEST_ASSERT_TRUE(icon_area.x2 < sound_area.x1);
    lv_obj_get_coords(hg_clock_screen_test_alarm_label(), &time_area);
    TEST_ASSERT_TRUE(sound_area.x1 > time_area.x2 || sound_area.y2 < time_area.y1);
    TEST_ASSERT_TRUE(sound_area.x1 >= 0 && sound_area.x2 < lv_disp_get_hor_res(nullptr));
    uint32_t started = millis();
    while (millis() - started < HG_ALARM_EDIT_BLINK_MS + 20U) {
        hg_display_poll();
        lv_timer_handler();
        delay(1);
    }
    TEST_ASSERT_EQUAL(lv_color_to32(hg_theme_bg()),
                      lv_color_to32(lv_obj_get_style_text_color(label, 0)));
    TEST_ASSERT_EQUAL(lv_color_to32(hg_theme_bg()),
                      lv_color_to32(lv_obj_get_style_text_color(icon, 0)));
    hold();
    step(1, false, true);
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_PLUCKED, alarm().sound);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    snap.alarm_edit = editor();
    hg_clock_screen_update(&snap);
    TEST_ASSERT_TRUE(lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN));
    TEST_ASSERT_TRUE(lv_obj_has_flag(icon, LV_OBJ_FLAG_HIDDEN));
    hold();
    step(1, false, true);
    short_press();
    short_press();
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_PLUCKED, editor().sound);
    step(1, false, false, 1);
    step(HG_ALARM_EDIT_IDLE_MS - 1);
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_PLUCKED, alarm().sound);
    step(1);
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_MALLETS, alarm().sound);
    hg_alarm_commit_edit(23, 59, HG_ALARM_SOUND_MALLETS);
}

void test_alarm_stop_consumes_hold_everywhere(void) {
    begin();
    hg_time_snapshot_t time;
    hg_time_get_snapshot(&time);
    TEST_ASSERT_TRUE(time.valid);
    hg_alarm_set_time(time.hour_24, time.minute);
    hg_alarm_set_enabled(true);
    hg_alarm_test_check_time(&time);
    TEST_ASSERT_TRUE(alarm().ringing);
    hg_navigator_show_page(HG_PAGE_WEATHER);
    step(1, true, false, 0, false, false, false);
    TEST_ASSERT_FALSE(alarm().ringing);
    TEST_ASSERT_TRUE(alarm().enabled);
    step(HG_ENCODER_HOLD_MS * 2, false, false, 0, false, false, false);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    step(1, false, true);
    TEST_ASSERT_TRUE(alarm().enabled);
}

static void app_event(bool pressed = false, bool released = false, int32_t delta = 0) {
    hg_input_event_t event = {millis(), hg_runtime_input_generation(), false,
                              pressed, delta, released};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    hg_app_loop();
}

static void app_hold(void) {
    uint32_t started = millis();
    app_event(true);
    while (millis() - started <= HG_ENCODER_HOLD_MS + 10U) {
        app_event();
    }
}

void test_alarm_editor_app_snapshot_handoff(void) {
    begin();
    hg_runtime_test_manual_input(true);
    hg_input_router_init();
    while (!hg_alarm_toggle_allowed(millis())) {
        hg_app_loop();
    }
    app_hold();
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_MINUTE, editor().field);
    lv_obj_t *label = hg_clock_screen_test_alarm_label();
    TEST_ASSERT_FALSE(lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN));
    app_event(false, true);
    app_event(false, false, 2);
    TEST_ASSERT_EQUAL(0, editor().hour);
    TEST_ASSERT_EQUAL(1, editor().minute);
    TEST_ASSERT_EQUAL_STRING("12:01am", lv_label_get_text(label));
    TEST_ASSERT_EQUAL(23, alarm().hour);
    TEST_ASSERT_EQUAL(59, alarm().minute);
    app_event(true);
    app_event(false, true);
    app_event(true);
    app_event(false, true);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_SOUND, editor().field);
    hg_alarm_sound_t original_sound = alarm().sound;
    app_event(false, false, 1);
    TEST_ASSERT_EQUAL((original_sound + 1) % HG_ALARM_SOUND_COUNT, editor().sound);
    TEST_ASSERT_EQUAL(original_sound, alarm().sound);
    TEST_ASSERT_EQUAL_STRING(editor().sound == HG_ALARM_SOUND_WOODEN ? "Wooden call" :
                             editor().sound == HG_ALARM_SOUND_PLUCKED ? "Plucked dawn" :
                                                                      "Warm mallets",
                             lv_label_get_text(hg_clock_screen_test_sound_label()));
    app_hold();
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
    TEST_ASSERT_EQUAL(0, alarm().hour);
    TEST_ASSERT_EQUAL(1, alarm().minute);
    TEST_ASSERT_EQUAL((original_sound + 1) % HG_ALARM_SOUND_COUNT, alarm().sound);
    TEST_ASSERT_FALSE(alarm().enabled);
    TEST_ASSERT_TRUE(lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN));
    app_event(false, true);
    TEST_ASSERT_FALSE(alarm().enabled);
    hg_alarm_commit_edit(0, 1, HG_ALARM_SOUND_MALLETS);
}

static hg_effects_snapshot_t feedback(void) {
    hg_effects_snapshot_t value;
    hg_effects_get_snapshot(&value);
    return value;
}

static void reset_feedback(void) {
    hg_effects_init();
    hg_effects_set_capabilities(false, true);
}

void test_alarm_action_cues(void) {
    begin();
    reset_feedback();
    short_press();
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ALARM_ENABLE, feedback().feedback);
    short_press();
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ALARM_DISABLE, feedback().feedback);
    hold();
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ALARM_ENTER, feedback().feedback);
    uint32_t generation = feedback().generation;
    step(HG_ENCODER_HOLD_MS);
    step(1, false, true);
    TEST_ASSERT_EQUAL_UINT32(generation, feedback().generation);
    step(1, false, false, 2);
    TEST_ASSERT_EQUAL_UINT32(generation, feedback().generation);
    step(1, false, false, 0, true);
    step(1, false, false, 0, false, true);
    TEST_ASSERT_EQUAL_UINT32(generation, feedback().generation);
    short_press();
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ALARM_FIELD, feedback().feedback);
    generation = feedback().generation;
    short_press();
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ALARM_FIELD, feedback().feedback);
    TEST_ASSERT_EQUAL_UINT32(generation + 1U, feedback().generation);
    hold();
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ALARM_SAVE, feedback().feedback);
    generation = feedback().generation;
    step(1, false, true);
    TEST_ASSERT_EQUAL_UINT32(generation, feedback().generation);
    hold();
    step(1, false, true);
    step(HG_ALARM_EDIT_IDLE_MS - 1U);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ALARM_ENTER, feedback().feedback);
    step(1);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ALARM_SAVE, feedback().feedback);
    TEST_ASSERT_EQUAL(HG_ALARM_EDIT_NONE, editor().field);
}

void test_alarm_cues_ignore_consumed_input(void) {
    begin();
    reset_feedback();
    step(1, false, false, 5);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, feedback().feedback);
    step(1, true, false, 0, false, false, false);
    step(HG_ENCODER_HOLD_MS);
    step(1, false, true);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, feedback().feedback);
    step(1, true);
    hg_runtime_set_navigation_allowed(false);
    hg_runtime_set_navigation_allowed(true);
    step(HG_ENCODER_HOLD_MS);
    step(1, false, true);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, feedback().feedback);
    step(1, true);
    now_ms += HG_INPUT_MAX_AGE_MS + 1U;
    hg_input_router_test_update(true, now_ms);
    step(HG_ENCODER_HOLD_MS);
    step(1, false, true);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, feedback().feedback);

    enter_off();
    hold();
    step(1, false, true);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, feedback().feedback);
    enter_off();
    step(1, false, false, 2);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, feedback().feedback);
    enter_off();
    step(1, false, false, 0, true);
    step(1, false, false, 0, false, true);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, feedback().feedback);
    hg_navigator_show_page(HG_PAGE_WEATHER);
    short_press();
    hold();
    step(1, false, true);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, feedback().feedback);

    hg_alarm_snapshot_t value = alarm();
    hg_alarm_set_time(value.hour, value.minute);
    hg_alarm_set_enabled(true);
    hg_time_snapshot_t due = {true, value.hour, value.minute, 0};
    hg_alarm_test_check_time(&due);
    TEST_ASSERT_TRUE(alarm().ringing);
    step(1, false, false, 0, true);
    step(1, false, false, 0, false, true);
    TEST_ASSERT_TRUE(alarm().snoozed);
    hold();
    step(1, false, true);
    TEST_ASSERT_FALSE(alarm().snoozed);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, feedback().feedback);
}

void alarm_edit_test_cleanup(void) {
    if (!active) {
        return;
    }
    active = false;
    hg_input_router_init();
    hg_alarm_set_enabled(false);
    hg_runtime_test_manual_input(true);
    hg_input_event_t discarded;
    while (hg_runtime_take_input(&discarded)) {}
    hg_display_mode_init();
    set_lux(100.0f);
    hg_clock_snapshot_t snap = {};
    snap.display_active = true;
    hg_clock_screen_update(&snap);
}
