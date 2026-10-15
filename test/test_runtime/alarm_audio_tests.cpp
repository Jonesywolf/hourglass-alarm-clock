#include <Arduino.h>
#include <esp_heap_caps.h>
#include <unity.h>

#include "hg/app.h"
#include "hg/app/runtime.h"
#include "hg/config.h"
#include "hg/hal/rtc.h"
#include "hg/services/alarm.h"
#include "hg/services/effects.h"
#include "hg/services/sound.h"
#include "hg/services/store.h"
#include "hg/services/time.h"
#include "hg/ui/overlays.h"

static void run_app_ms(uint32_t duration_ms) {
    uint32_t started = millis();
    while (millis() - started < duration_ms) {
        hg_app_loop();
    }
}

static void inject(bool tapped, bool pressed) {
    hg_input_event_t event = {millis(), hg_runtime_input_generation(), tapped, pressed, 0};
    TEST_ASSERT_TRUE(hg_runtime_test_input(&event));
    hg_app_loop();
}

static int minutes_of_day(void) {
    hg_time_snapshot_t time;
    hg_time_get_snapshot(&time);
    return time.hour_24 * 60 + time.minute;
}

/* Also used by stress_tests.cpp. Resets the alarm so it can ring again within one minute. */
void test_ring_alarm_now(void) {
    hg_time_snapshot_t time;
    hg_time_get_snapshot(&time);
    if (!time.valid) {
        hg_time_snapshot_t fallback = {true, 7, 15, 0};
        TEST_ASSERT_TRUE(hg_rtc_set_time(&fallback));
        hg_time_get_snapshot(&time);
    }
    hg_alarm_set_enabled(false);
    hg_alarm_set_time(time.hour_24, time.minute);
    hg_alarm_set_enabled(true);
    hg_alarm_update();
    hg_alarm_snapshot_t alarm;
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_TRUE_MESSAGE(alarm.ringing, "alarm did not ring (minute rollover?)");
}

static void wait_overlay_gone(void) {
    uint32_t started = millis();
    while (hg_overlays_alarm_visible() && millis() - started < HG_ALARM_CONFIRM_MS + 3000U) {
        hg_app_loop();
    }
    TEST_ASSERT_FALSE(hg_overlays_alarm_visible());
}

void test_sound_mapping_and_availability(void) {
    hg_effects_snapshot_t effects = {};
    TEST_ASSERT_EQUAL(HG_SOUND_NONE, hg_sound_for_effects(&effects));
    effects.phase = HG_EFFECT_RINGING;
    TEST_ASSERT_EQUAL(HG_SOUND_ALARM, hg_sound_for_effects(&effects));
    effects.phase = HG_EFFECT_SUNRISE;
    TEST_ASSERT_EQUAL(HG_SOUND_NONE, hg_sound_for_effects(&effects));
    effects.phase = HG_EFFECT_NORMAL;
    effects.feedback = HG_FEEDBACK_NAVIGATE;
    TEST_ASSERT_EQUAL(HG_SOUND_UI_NAVIGATE, hg_sound_for_effects(&effects));
    TEST_ASSERT_TRUE(hg_sound_available(HG_SOUND_ALARM));
    /* Feedback sounds are reserved ids with no sample yet. */
    TEST_ASSERT_FALSE(hg_sound_available(HG_SOUND_UI_NAVIGATE));
    TEST_ASSERT_FALSE(hg_sound_available(HG_SOUND_UI_ALARM_EDIT));
    TEST_ASSERT_FALSE(hg_sound_available(HG_SOUND_NONE));
    hg_sound_player_t player;
    TEST_ASSERT_FALSE(hg_sound_player_start(&player, HG_SOUND_UI_NAVIGATE, HG_AUDIO_SAMPLE_RATE));
    int16_t buffer[64];
    TEST_ASSERT_EQUAL_UINT32(0, hg_sound_player_render(&player, buffer, 64));
    TEST_ASSERT_FALSE(hg_sound_player_start(&player, HG_SOUND_ALARM, HG_AUDIO_SAMPLE_RATE / 2));
}

void test_sound_ramp_loop_and_limits(void) {
    const float start = HG_ALARM_VOLUME_START_PCT / 100.0f;
    const float cap = HG_ALARM_VOLUME_MAX_PCT / 100.0f;
    const uint32_t rate = HG_AUDIO_SAMPLE_RATE;
    const uint32_t ramp_frames = HG_ALARM_RAMP_MS * (rate / 1000U);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, hg_sound_gain_at(HG_SOUND_ALARM, 0, rate));
    float previous = 0.0f;
    for (uint32_t frame = 0; frame <= ramp_frames + rate; frame += rate / 10U) {
        float gain = hg_sound_gain_at(HG_SOUND_ALARM, frame, rate);
        TEST_ASSERT_TRUE(gain >= previous - 0.0001f);
        TEST_ASSERT_TRUE(gain <= cap + 0.0001f);
        previous = gain;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, cap, hg_sound_gain_at(HG_SOUND_ALARM, ramp_frames, rate));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, cap, hg_sound_gain_at(HG_SOUND_ALARM, ramp_frames * 4U, rate));
    uint32_t after_fade = HG_AUDIO_FADE_IN_MS * (rate / 1000U);
    TEST_ASSERT_TRUE(hg_sound_gain_at(HG_SOUND_ALARM, after_fade, rate) >= start - 0.0001f);

    hg_sound_player_t player;
    TEST_ASSERT_TRUE(hg_sound_player_start(&player, HG_SOUND_ALARM, rate));
    TEST_ASSERT_TRUE(player.loop);
    int16_t buffer[HG_AUDIO_CHUNK_FRAMES];
    uint32_t rendered = 0;
    bool wrapped = false;
    uint32_t last_position = 0;
    int32_t peak = 0;
    /* Render past two loop points; looping never runs dry. */
    while (rendered < player.frames * 2U + HG_AUDIO_CHUNK_FRAMES) {
        size_t frames = hg_sound_player_render(&player, buffer, HG_AUDIO_CHUNK_FRAMES);
        TEST_ASSERT_EQUAL_UINT32(HG_AUDIO_CHUNK_FRAMES, frames);
        for (size_t i = 0; i < frames; ++i) {
            int32_t magnitude = buffer[i] < 0 ? -(int32_t)buffer[i] : buffer[i];
            if (magnitude > peak) {
                peak = magnitude;
            }
        }
        rendered += (uint32_t)frames;
        wrapped = wrapped || player.position < last_position;
        last_position = player.position;
    }
    TEST_ASSERT_TRUE(wrapped);
    TEST_ASSERT_EQUAL_UINT32(rendered, player.elapsed);
    TEST_ASSERT_TRUE(peak <= (int32_t)(32768.0f * cap) + 1);
}

void test_alarm_snooze_keeps_saved_time(void) {
    test_ring_alarm_now();
    hg_alarm_snapshot_t base;
    hg_alarm_get_snapshot(&base);
    hg_store_status_t before;
    hg_store_get_status(&before);
    int now_before = minutes_of_day();
    hg_alarm_snooze();
    int now_after = minutes_of_day();
    hg_alarm_snapshot_t snoozed;
    hg_alarm_get_snapshot(&snoozed);
    TEST_ASSERT_FALSE(snoozed.ringing);
    TEST_ASSERT_TRUE(snoozed.snoozed);
    TEST_ASSERT_TRUE(snoozed.enabled);
    int ring_at = snoozed.hour * 60 + snoozed.minute;
    int accepted_a = (now_before + HG_ALARM_SNOOZE_MIN) % (24 * 60);
    int accepted_b = (now_after + HG_ALARM_SNOOZE_MIN) % (24 * 60);
    TEST_ASSERT_TRUE(ring_at == accepted_a || ring_at == accepted_b);
    /* Snoozing never persists the temporary time. */
    hg_store_status_t after;
    hg_store_get_status(&after);
    TEST_ASSERT_EQUAL_UINT32(before.requested_generation, after.requested_generation);
    TEST_ASSERT_FALSE(hg_alarm_toggle_allowed(millis()));
    hg_alarm_snooze();
    hg_alarm_snapshot_t unchanged;
    hg_alarm_get_snapshot(&unchanged);
    TEST_ASSERT_EQUAL(snoozed.hour, unchanged.hour);
    TEST_ASSERT_EQUAL(snoozed.minute, unchanged.minute);

    hg_alarm_dismiss();
    hg_alarm_snapshot_t restored;
    hg_alarm_get_snapshot(&restored);
    TEST_ASSERT_FALSE(restored.snoozed);
    TEST_ASSERT_FALSE(restored.ringing);
    TEST_ASSERT_TRUE(restored.enabled);
    TEST_ASSERT_EQUAL(base.hour, restored.hour);
    TEST_ASSERT_EQUAL(base.minute, restored.minute);
    hg_alarm_set_enabled(false);
}

void test_encoder_stops_alarm_without_disabling(void) {
    hg_app_test_hide_boot_overlay(true);
    test_ring_alarm_now();
    hg_alarm_snapshot_t base;
    hg_alarm_get_snapshot(&base);
    hg_app_loop();
    TEST_ASSERT_TRUE(hg_overlays_alarm_visible());

    inject(true, false);
    hg_alarm_snapshot_t alarm;
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_TRUE(alarm.snoozed);
    TEST_ASSERT_FALSE(alarm.ringing);
    TEST_ASSERT_TRUE(hg_overlays_alarm_visible());

    inject(false, true);
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_FALSE(alarm.snoozed);
    TEST_ASSERT_TRUE(alarm.enabled);
    TEST_ASSERT_EQUAL(base.hour, alarm.hour);
    TEST_ASSERT_EQUAL(base.minute, alarm.minute);

    /* Mashing the knob inside the lockout must not disable tomorrow's alarm. */
    inject(false, true);
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_TRUE(alarm.enabled);
    wait_overlay_gone();

    /* After the lockout the knob toggles the alarm as before. */
    while (!hg_alarm_toggle_allowed(millis())) {
        hg_app_loop();
    }
    inject(false, true);
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_FALSE(alarm.enabled);

    /* Pressing while ringing stops it and leaves the alarm enabled. */
    test_ring_alarm_now();
    hg_app_loop();
    inject(false, true);
    hg_alarm_get_snapshot(&alarm);
    TEST_ASSERT_FALSE(alarm.ringing);
    TEST_ASSERT_FALSE(alarm.snoozed);
    TEST_ASSERT_TRUE(alarm.enabled);
    wait_overlay_gone();
    hg_alarm_set_enabled(false);
}

void test_audio_worker_plays_acks_and_stops(void) {
    hg_app_test_hide_boot_overlay(true);
    TEST_ASSERT_TRUE_MESSAGE(hg_runtime_audio_running(), "speaker/audio worker not running");
    hg_runtime_diagnostics_t diagnostics;
    hg_runtime_get_diagnostics(&diagnostics);
    uint32_t underruns_before = diagnostics.audio_underruns;
    uint32_t errors_before = diagnostics.audio_write_errors;

    test_ring_alarm_now();
    uint32_t started = millis();
    hg_effects_snapshot_t effects = {};
    hg_audio_status_t status = {};
    bool active = false;
    while (millis() - started < 1000U) {
        hg_app_loop();
        hg_effects_get_snapshot(&effects);
        active = hg_runtime_get_audio_status(&status) && status.generation == effects.generation &&
                 status.active;
        if (active) {
            break;
        }
    }
    uint32_t start_ms = millis() - started;
    TEST_ASSERT_TRUE(active);
    hg_app_loop();
    hg_effects_get_snapshot(&effects);
    TEST_ASSERT_TRUE(effects.audio_active);
    TEST_ASSERT_TRUE(effects.timing_sensitive);

    run_app_ms(2000U);
    hg_effects_get_snapshot(&effects);
    TEST_ASSERT_TRUE(effects.audio_active);
    uint32_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t min_internal = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    uint32_t largest_dma = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);

    inject(true, false);
    uint32_t stop_requested = millis();
    bool stopped = false;
    while (millis() - stop_requested < 500U) {
        hg_app_loop();
        hg_effects_get_snapshot(&effects);
        if (!effects.audio_active && !effects.stop_pending) {
            stopped = true;
            break;
        }
    }
    uint32_t stop_ms = millis() - stop_requested;
    hg_runtime_get_diagnostics(&diagnostics);
    Serial.printf("AUDIO start=%lums stop_ack=%lums underruns=%lu errors=%lu stack_free=%lu\n",
                  (unsigned long)start_ms, (unsigned long)stop_ms,
                  (unsigned long)(diagnostics.audio_underruns - underruns_before),
                  (unsigned long)(diagnostics.audio_write_errors - errors_before),
                  (unsigned long)diagnostics.stack_free_bytes[3]);
    Serial.printf("AUDIO HEAP during playback: internal free=%lu min=%lu largest DMA block=%lu\n",
                  (unsigned long)free_internal, (unsigned long)min_internal,
                  (unsigned long)largest_dma);
    TEST_ASSERT_TRUE(stopped);
    TEST_ASSERT_LESS_THAN_UINT32(300, stop_ms);
    TEST_ASSERT_EQUAL_UINT32(0, diagnostics.audio_underruns - underruns_before);
    TEST_ASSERT_EQUAL_UINT32(0, diagnostics.audio_write_errors - errors_before);
    TEST_ASSERT_GREATER_THAN_UINT32(256, diagnostics.stack_free_bytes[3]);
    wait_overlay_gone();
    hg_alarm_dismiss();
    hg_alarm_set_enabled(false);
}
