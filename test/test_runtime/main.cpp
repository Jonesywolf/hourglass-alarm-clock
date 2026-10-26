#include <Arduino.h>
#include <unity.h>

#include "hg/log.h"
#include "hg/app/runtime.h"
#include "hg/services/store.h"

void test_effect_defaults_and_unavailable(void);
void test_alarm_preempts_feedback_without_replay(void);
void test_feedback_expiry_and_stop_generation(void);
void test_aqi_opt_in_and_applied_state(void);
void test_nonwaiting_time_and_fallback(void);
void test_brightness_keeps_last_good_classification(void);
void test_timing_percentiles(void);
void test_worker_creation_failure_cleanup(void);
void test_app_startup(void);
void test_input_queue_and_overlay_generation(void);
void test_stale_samples_and_pause_generation(void);
void test_save_coalescing_and_acknowledgement(void);
void test_save_retry_and_admission(void);
void test_navigation_and_encoder_routing(void);
void test_alarm_during_slide_and_overlay(void);
void test_sound_mapping_and_availability(void);
void test_sound_ramp_loop_and_limits(void);
void test_alarm_snooze_keeps_saved_time(void);
void test_encoder_stops_alarm_without_disabling(void);
void test_audio_worker_plays_acks_and_stops(void);
void test_combined_load_frame_timing(void);
void test_audio_playback_under_load(void);
void test_real_nvs_admission_timing(void);
void test_alarm_editor_fields_and_commit(void);
void test_alarm_editor_touch_and_idle_save(void);
void test_alarm_editor_suppression_and_matching_commit(void);
void test_alarm_editor_page_and_generation_isolation(void);
void test_alarm_editor_lost_release_and_wraparound(void);
void test_nighttime_wake_gestures(void);
void test_nighttime_timeouts_and_alarm_guard(void);
void test_alarm_editor_flashing_presentation(void);
void test_alarm_stop_consumes_hold_everywhere(void);
void test_alarm_editor_app_snapshot_handoff(void);
void test_alarm_cue_replacement_and_precedence(void);
void test_alarm_cue_waveforms(void);
void test_alarm_action_cues(void);
void test_alarm_cues_ignore_consumed_input(void);
void test_audio_cue_completion_and_preemption(void);
void test_audio_cue_failures_do_not_retry(void);
void test_audio_cues_under_load(void);
void alarm_edit_test_cleanup(void);
void volume_test_cleanup(void);
void test_volume_steps_clamps_and_timeout(void);
void test_volume_context_and_snooze_isolation(void);
void test_volume_record_compatibility_and_coalescing(void);
void test_volume_scaled_pcm_and_unchanged_cues(void);
void test_volume_panel_animation_and_cancellation(void);
void test_volume_immediate_app_handoff_and_editing(void);
void test_volume_animation_under_load(void);

void setUp(void) {}
void tearDown(void) {
    alarm_edit_test_cleanup();
    volume_test_cleanup();
    hg_runtime_test_audio_failure(false, false);
    hg_runtime_test_load(0, 0, 0);
    hg_store_set_flash_allowed(false);
}

void setup(void) {
    Serial.begin(115200);
    delay(1500);
    hg_log_init();
    UNITY_BEGIN();
    RUN_TEST(test_effect_defaults_and_unavailable);
    RUN_TEST(test_alarm_preempts_feedback_without_replay);
    RUN_TEST(test_feedback_expiry_and_stop_generation);
    RUN_TEST(test_aqi_opt_in_and_applied_state);
    RUN_TEST(test_alarm_cue_replacement_and_precedence);
    RUN_TEST(test_nonwaiting_time_and_fallback);
    RUN_TEST(test_brightness_keeps_last_good_classification);
    RUN_TEST(test_timing_percentiles);
    RUN_TEST(test_worker_creation_failure_cleanup);
    RUN_TEST(test_app_startup);
    RUN_TEST(test_input_queue_and_overlay_generation);
    RUN_TEST(test_stale_samples_and_pause_generation);
    RUN_TEST(test_save_coalescing_and_acknowledgement);
    RUN_TEST(test_save_retry_and_admission);
    RUN_TEST(test_navigation_and_encoder_routing);
    RUN_TEST(test_alarm_during_slide_and_overlay);
    RUN_TEST(test_alarm_editor_fields_and_commit);
    RUN_TEST(test_alarm_editor_touch_and_idle_save);
    RUN_TEST(test_alarm_editor_suppression_and_matching_commit);
    RUN_TEST(test_alarm_editor_page_and_generation_isolation);
    RUN_TEST(test_alarm_editor_lost_release_and_wraparound);
    RUN_TEST(test_nighttime_wake_gestures);
    RUN_TEST(test_nighttime_timeouts_and_alarm_guard);
    RUN_TEST(test_alarm_editor_flashing_presentation);
    RUN_TEST(test_alarm_stop_consumes_hold_everywhere);
    RUN_TEST(test_alarm_editor_app_snapshot_handoff);
    RUN_TEST(test_alarm_action_cues);
    RUN_TEST(test_alarm_cues_ignore_consumed_input);
    RUN_TEST(test_volume_steps_clamps_and_timeout);
    RUN_TEST(test_volume_context_and_snooze_isolation);
    RUN_TEST(test_volume_record_compatibility_and_coalescing);
    RUN_TEST(test_volume_scaled_pcm_and_unchanged_cues);
    RUN_TEST(test_volume_panel_animation_and_cancellation);
    RUN_TEST(test_volume_immediate_app_handoff_and_editing);
    RUN_TEST(test_sound_mapping_and_availability);
    RUN_TEST(test_sound_ramp_loop_and_limits);
    RUN_TEST(test_alarm_cue_waveforms);
    RUN_TEST(test_alarm_snooze_keeps_saved_time);
    RUN_TEST(test_encoder_stops_alarm_without_disabling);
    RUN_TEST(test_audio_worker_plays_acks_and_stops);
    RUN_TEST(test_audio_cue_completion_and_preemption);
    RUN_TEST(test_audio_cue_failures_do_not_retry);
    RUN_TEST(test_combined_load_frame_timing);
    RUN_TEST(test_audio_playback_under_load);
    RUN_TEST(test_audio_cues_under_load);
    RUN_TEST(test_volume_animation_under_load);
    RUN_TEST(test_real_nvs_admission_timing);
    UNITY_END();
}

void loop(void) {
    delay(1000);
}
