#include <Arduino.h>
#include <unity.h>

#include "hg/app.h"
#include "hg/app/input_router.h"
#include "hg/app/runtime.h"
#include "hg/config.h"
#include "hg/hal/rtc.h"
#include "hg/services/alarm.h"
#include "hg/services/brightness.h"
#include "hg/services/store.h"
#include "hg/services/time.h"
#include "hg/ui/boot_overlay.h"
#include "hg/ui/navigator.h"
#include "hg/ui/overlays.h"

static bool wait_for_save(uint32_t timeout_ms) {
    uint32_t start = millis();
    do {
        hg_store_status_t status;
        hg_store_get_status(&status);
        if (!status.pending && !status.writing) {
            return true;
        }
        delay(5);
    } while (millis() - start < timeout_ms);
    return false;
}

static void finish_slide(void) {
    uint32_t start = millis();
    while (hg_navigator_is_sliding() && millis() - start < 2000U) {
        hg_app_loop();
    }
}

void test_nonwaiting_time_and_fallback(void) {
    hg_rtc_init();
    hg_time_snapshot_t time;
    uint32_t started = micros();
    for (unsigned i = 0; i < 64; ++i) {
        hg_time_get_snapshot(&time);
    }
    TEST_ASSERT_LESS_THAN_UINT32(10000, micros() - started);
    TEST_ASSERT_FALSE(time.valid);
    hg_time_snapshot_t fallback = {true, 7, 5, 10};
    TEST_ASSERT_TRUE(hg_rtc_set_time(&fallback));
    hg_time_get_snapshot(&time);
    TEST_ASSERT_TRUE(time.valid);
    TEST_ASSERT_EQUAL(7, time.hour_24);
    TEST_ASSERT_EQUAL(5, time.minute);
}

void test_brightness_keeps_last_good_classification(void) {
    hg_brightness_init();
    hg_light_snapshot_t light = {true, 1.0f, 0, HG_SAMPLE_READY};
    hg_brightness_update(&light);
    TEST_ASSERT_EQUAL(HG_LIGHT_DARK, hg_brightness_get_light_level());
    light.lux = 100.0f;
    light.status = HG_SAMPLE_ERROR;
    hg_brightness_update(&light);
    TEST_ASSERT_EQUAL(HG_LIGHT_DARK, hg_brightness_get_light_level());
    light.status = HG_SAMPLE_STALE;
    hg_brightness_update(&light);
    TEST_ASSERT_EQUAL(HG_LIGHT_DARK, hg_brightness_get_light_level());
    light.status = HG_SAMPLE_READY;
    hg_brightness_update(&light);
    TEST_ASSERT_EQUAL(HG_LIGHT_BRIGHT, hg_brightness_get_light_level());
    light.lux = 25.0f;
    hg_brightness_update(&light);
    TEST_ASSERT_EQUAL(HG_LIGHT_NEUTRAL, hg_brightness_get_light_level());
}

void test_worker_creation_failure_cleanup(void) {
    hg_store_init();
    for (int i = 0; i < 4; ++i) {
        hg_runtime_test_fail_worker(i);
        TEST_ASSERT_FALSE(hg_runtime_start());
        TEST_ASSERT_NOT_NULL(hg_runtime_fault());
        delay(50);
    }
    hg_runtime_test_fail_worker(-1);
}

void test_app_startup(void) {
    hg_store_test_writer(0, false);
    hg_app_setup();
    TEST_ASSERT_NULL(hg_runtime_fault());
    hg_alarm_set_enabled(false);
    hg_app_test_hide_boot_overlay(true);
    uint32_t started = millis();
    while (millis() - started < 1500U) {
        hg_app_loop();
    }
    TEST_ASSERT_NULL(hg_runtime_fault());
    hg_runtime_diagnostics_t diagnostics;
    hg_runtime_get_diagnostics(&diagnostics);
    TEST_ASSERT_GREATER_THAN_UINT32(100, diagnostics.capture_gap.samples);
    for (unsigned i = 0; i < 3; ++i) {
        TEST_ASSERT_GREATER_THAN_UINT32(256, diagnostics.stack_free_bytes[i]);
    }
    if (hg_runtime_audio_running()) {
        TEST_ASSERT_GREATER_THAN_UINT32(256, diagnostics.stack_free_bytes[3]);
    }
    hg_runtime_test_manual_input(true);
}

void test_input_queue_and_overlay_generation(void) {
    hg_input_event_t event;
    while (hg_runtime_take_input(&event)) {}
    hg_runtime_reset_diagnostics();
    hg_runtime_set_navigation_allowed(false);
    event = {millis(), hg_runtime_input_generation(), true, false, 0};
    for (unsigned i = 0; i < HG_INPUT_QUEUE_CAPACITY; ++i) {
        TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    }
    TEST_ASSERT_FALSE(hg_runtime_test_input(&event));
    hg_runtime_diagnostics_t diagnostics;
    hg_runtime_get_diagnostics(&diagnostics);
    TEST_ASSERT_EQUAL_UINT32(1, diagnostics.input_overflows);
    TEST_ASSERT_EQUAL_UINT32(HG_INPUT_QUEUE_CAPACITY, diagnostics.input_queue_high_water);
    hg_runtime_set_navigation_allowed(true);
    hg_page_id_t page = hg_navigator_current();
    hg_input_router_update(true);
    TEST_ASSERT_EQUAL(page, hg_navigator_current());
    TEST_ASSERT_FALSE(hg_runtime_take_input(&event));
    event = {millis() - HG_INPUT_MAX_AGE_MS - 1, hg_runtime_input_generation(), true, true, 5};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    TEST_ASSERT_TRUE(hg_runtime_take_input(&event));
    TEST_ASSERT_FALSE(event.tapped);
    TEST_ASSERT_FALSE(event.encoder_pressed);
    TEST_ASSERT_EQUAL_INT32(0, event.encoder_delta);
    hg_runtime_get_diagnostics(&diagnostics);
    TEST_ASSERT_EQUAL_UINT32(1, diagnostics.expired_inputs);
    event = {millis() - UINT32_MAX / 1000U - 10U, hg_runtime_input_generation(), true, false, 0};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    TEST_ASSERT_TRUE(hg_runtime_take_input(&event));
    hg_runtime_get_diagnostics(&diagnostics);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, diagnostics.input_age.max_us);
}

void test_stale_samples_and_pause_generation(void) {
    hg_runtime_test_load(250, 0, 0);
    delay(40);
    hg_sensor_snapshot_t fake = {};
    fake.light = {true, 2.0f, millis() - HG_SENSOR_STALE_MS, HG_SAMPLE_READY};
    fake.indoor.valid = true;
    fake.indoor.pm2_5 = 12.0f;
    fake.indoor.sampled_ms = millis();
    fake.indoor.status = HG_SAMPLE_READY;
    fake.policy_generation = hg_runtime_set_indoor_paused(false);
    hg_runtime_test_sensor_cache(&fake);
    hg_sensor_snapshot_t observed;
    hg_runtime_get_sensors(&observed);
    TEST_ASSERT_EQUAL(HG_SAMPLE_STALE, observed.light.status);
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, observed.indoor.status);
    hg_runtime_set_indoor_paused(true);
    hg_runtime_get_sensors(&observed);
    TEST_ASSERT_EQUAL(HG_SAMPLE_PAUSED, observed.indoor.status);
    hg_runtime_set_indoor_paused(false);
    hg_runtime_get_sensors(&observed);
    TEST_ASSERT_EQUAL(HG_SAMPLE_STALE, observed.indoor.status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, observed.indoor.pm2_5);
    fake.indoor.status = HG_SAMPLE_ERROR;
    hg_runtime_test_sensor_cache(&fake);
    hg_runtime_get_sensors(&observed);
    TEST_ASSERT_EQUAL(HG_SAMPLE_ERROR, observed.indoor.status);
    hg_runtime_test_load(0, 0, 0);
}

void test_save_coalescing_and_acknowledgement(void) {
    hg_store_set_flash_allowed(false);
    hg_store_test_writer(100, false);
    hg_store_status_t before, status;
    hg_store_get_status(&before);
    hg_alarm_snapshot_t value = {true, false, 9, 1, false, HG_ALARM_VOLUME_DEFAULT_PCT};
    hg_store_request_save_alarm(&value);
    value.sound = HG_ALARM_SOUND_WOODEN;
    hg_store_request_save_alarm(&value);
    hg_store_request_save_alarm(&value);
    hg_store_get_status(&status);
    TEST_ASSERT_EQUAL_UINT32(before.requested_generation + 2, status.requested_generation);
    TEST_ASSERT_TRUE(status.pending);
    hg_store_set_flash_allowed(true);
    uint32_t started = millis();
    do {
        hg_store_get_status(&status);
        if (status.writing) {
            break;
        }
        delay(1);
    } while (millis() - started < 1000U);
    TEST_ASSERT_TRUE(status.writing);
    uint32_t in_flight = status.requested_generation;
    value.sound = HG_ALARM_SOUND_PLUCKED;
    hg_store_request_save_alarm(&value);
    hg_store_set_flash_allowed(false);
    delay(150);
    hg_store_get_status(&status);
    TEST_ASSERT_FALSE(status.writing);
    TEST_ASSERT_TRUE(status.pending);
    TEST_ASSERT_EQUAL_UINT32(in_flight, status.saved_generation);
    TEST_ASSERT_EQUAL_UINT32(in_flight + 1, status.requested_generation);
    hg_store_set_flash_allowed(true);
    TEST_ASSERT_TRUE(wait_for_save(1000));
    hg_store_set_flash_allowed(false);
    hg_store_test_writer(0, false);
}

void test_save_retry_and_admission(void) {
    hg_store_test_writer(0, true);
    hg_alarm_snapshot_t value = {true, false, 9, 4, false, HG_ALARM_VOLUME_DEFAULT_PCT,
                                  HG_ALARM_SOUND_WOODEN};
    hg_store_status_t before, status;
    hg_store_get_status(&before);
    hg_store_request_save_alarm(&value);
    hg_store_set_flash_allowed(true);
    uint32_t started = millis();
    do {
        hg_store_get_status(&status);
        if (status.failures > before.failures) {
            break;
        }
        delay(5);
    } while (millis() - started < 1000U);
    TEST_ASSERT_GREATER_THAN_UINT32(before.failures, status.failures);
    TEST_ASSERT_TRUE(status.pending);
    hg_store_set_flash_allowed(false);
    hg_store_test_writer(0, false);
    delay(HG_STORE_RETRY_MS + 50U);
    hg_store_get_status(&status);
    TEST_ASSERT_TRUE(status.pending);
    TEST_ASSERT_EQUAL_UINT32(before.saved_generation, status.saved_generation);
    hg_store_set_flash_allowed(true);
    TEST_ASSERT_TRUE(wait_for_save(1000));
    hg_store_set_flash_allowed(false);
}

void test_navigation_and_encoder_routing(void) {
    hg_app_test_hide_boot_overlay(true);
    hg_navigator_show_page(HG_PAGE_CLOCK);
    finish_slide();
    hg_app_loop();
    hg_input_event_t event = {millis(), hg_runtime_input_generation(), true, false, 0};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    hg_app_loop();
    TEST_ASSERT_EQUAL(HG_PAGE_WEATHER, hg_navigator_current());
    event.captured_ms = millis();
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    hg_app_loop();
    TEST_ASSERT_EQUAL(HG_PAGE_CLOCK, hg_navigator_current());
    finish_slide();
    hg_alarm_set_time(10, 13);
    hg_alarm_set_enabled(true);
    event = {millis(), hg_runtime_input_generation(), false, false, 3};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    hg_app_loop();
    hg_alarm_snapshot_t alarm;
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_EQUAL(10, alarm.hour);
    TEST_ASSERT_EQUAL(13, alarm.minute);
    event = {millis(), hg_runtime_input_generation(), false, true, 0};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    hg_app_loop();
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_TRUE(alarm.enabled);
    event = {millis(), hg_runtime_input_generation(), false, false, 0, true};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    hg_app_loop();
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_FALSE(alarm.enabled);
    event = {millis(), hg_runtime_input_generation(), false, false, 3};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    hg_app_loop();
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_EQUAL(13, alarm.minute);
}

void test_alarm_during_slide_and_overlay(void) {
    hg_time_snapshot_t time;
    hg_time_get_snapshot(&time);
    if (!time.valid) {
        hg_time_snapshot_t fallback = {true, 7, 15, 0};
        TEST_ASSERT_TRUE(hg_rtc_set_time(&fallback));
        hg_time_get_snapshot(&time);
    }
    hg_alarm_set_time(time.hour_24, time.minute);
    hg_alarm_set_enabled(true);
    hg_navigator_show_page(HG_PAGE_WEATHER);
    hg_alarm_update();
    hg_app_loop();
    hg_alarm_snapshot_t alarm;
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_TRUE(alarm.ringing);
    /* The alarm screen overlays whichever page is showing; no page jump is needed. */
    TEST_ASSERT_TRUE(hg_overlays_alarm_visible());
    hg_wifi_snapshot_t disconnected = {};
    hg_time_snapshot_t invalid = {};
    hg_boot_overlay_update(&disconnected, &invalid, "test");
    TEST_ASSERT_TRUE(hg_boot_overlay_is_active());
    hg_input_event_t tap = {millis(), hg_runtime_input_generation(), true, false, 0};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&tap));
    hg_app_loop();
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_FALSE(alarm.ringing);
    TEST_ASSERT_TRUE(alarm.snoozed);
    TEST_ASSERT_TRUE(hg_boot_overlay_is_active());
    hg_alarm_set_enabled(false);
    hg_app_test_hide_boot_overlay(true);
    finish_slide();
    uint32_t settle = millis();
    while (hg_overlays_alarm_visible() && millis() - settle < HG_ALARM_CONFIRM_MS + 3000U) {
        hg_app_loop();
    }
}
