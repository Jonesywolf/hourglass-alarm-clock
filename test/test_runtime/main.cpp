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
void test_combined_load_frame_timing(void);
void test_real_nvs_admission_timing(void);

void setUp(void) {}
void tearDown(void) {
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
    RUN_TEST(test_combined_load_frame_timing);
    RUN_TEST(test_real_nvs_admission_timing);
    UNITY_END();
}

void loop(void) {
    delay(1000);
}
