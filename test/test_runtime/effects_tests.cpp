#include <unity.h>

#include "hg/config.h"
#include "hg/diagnostics.h"
#include "hg/services/effects.h"

void test_effect_defaults_and_unavailable(void) {
    hg_effects_init();
    TEST_ASSERT_EQUAL(HG_FEEDBACK_UNAVAILABLE,
                      hg_effects_request_feedback(HG_FEEDBACK_NAVIGATE, 0));
    hg_effects_snapshot_t state;
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_EQUAL(HG_EFFECT_NORMAL, state.phase);
    TEST_ASSERT_FALSE(state.pause_indoor);
    TEST_ASSERT_FALSE(state.leds_active);
    TEST_ASSERT_FALSE(state.audio_active);
    TEST_ASSERT_FALSE(hg_effects_report_applied(state.generation, true, false, false));
}

void test_alarm_preempts_feedback_without_replay(void) {
    hg_effects_init();
    hg_effects_set_capabilities(true, true);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ACCEPTED,
                      hg_effects_request_feedback(HG_FEEDBACK_NAVIGATE, 0));
    hg_effects_set_alarm(true, false);
    hg_effects_snapshot_t state;
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_EQUAL(HG_EFFECT_SUNRISE, state.phase);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, state.feedback);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_PREEMPTED,
                      hg_effects_request_feedback(HG_FEEDBACK_ALARM_EDIT, 500));
    hg_effects_set_alarm(true, true);
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_EQUAL(HG_EFFECT_RINGING, state.phase);
    uint32_t ringing_generation = state.generation;
    hg_effects_set_alarm(false, false);
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_TRUE(state.stop_pending);
    TEST_ASSERT_FALSE(hg_effects_report_applied(ringing_generation, true, true, false));
    TEST_ASSERT_FALSE(hg_effects_report_applied(state.generation, false, true, false));
    TEST_ASSERT_TRUE(hg_effects_report_applied(state.generation, false, false, false));
    hg_effects_update(10000);
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, state.feedback);
    TEST_ASSERT_FALSE(state.stop_pending);
}

void test_feedback_expiry_and_stop_generation(void) {
    hg_effects_init();
    hg_effects_set_capabilities(true, true);
    uint32_t start = UINT32_MAX - 100U;
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ACCEPTED,
                      hg_effects_request_feedback(HG_FEEDBACK_NAVIGATE, start));
    TEST_ASSERT_EQUAL(HG_FEEDBACK_RATE_LIMITED,
                      hg_effects_request_feedback(HG_FEEDBACK_ALARM_EDIT, start + 10U));
    hg_effects_snapshot_t state;
    hg_effects_get_snapshot(&state);
    uint32_t playing_generation = state.generation;
    TEST_ASSERT_TRUE(hg_effects_report_applied(playing_generation, true, true, false));
    hg_effects_update(start + HG_FEEDBACK_DURATION_MS - 1);
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NAVIGATE, state.feedback);
    hg_effects_update(start + HG_FEEDBACK_DURATION_MS);
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_TRUE(state.stop_pending);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_PREEMPTED,
                      hg_effects_request_feedback(HG_FEEDBACK_NAVIGATE, start + 500U));
    TEST_ASSERT_FALSE(hg_effects_report_applied(playing_generation, true, true, false));
    TEST_ASSERT_TRUE(hg_effects_report_applied(state.generation, false, false, false));
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ACCEPTED,
                      hg_effects_request_feedback(HG_FEEDBACK_ALARM_EDIT, start + 500U));
    hg_effects_cancel_feedback();
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, state.feedback);
    TEST_ASSERT_TRUE(state.stop_pending);
}

void test_aqi_opt_in_and_applied_state(void) {
    hg_effects_init();
    hg_effects_set_capabilities(true, true);
    hg_effects_snapshot_t state;
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_TRUE(hg_effects_report_applied(state.generation, true, false, false));
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_FALSE(state.pause_indoor);
    hg_effects_set_pause_indoor_for_leds(true);
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_TRUE(state.pause_indoor);
    TEST_ASSERT_TRUE(hg_effects_report_applied(state.generation, false, false, true));
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_TRUE(state.pause_indoor);
    TEST_ASSERT_TRUE(hg_effects_report_applied(state.generation, false, false, false));
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ACCEPTED,
                      hg_effects_request_feedback(HG_FEEDBACK_NAVIGATE, 1000));
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_FALSE(state.pause_indoor);
    TEST_ASSERT_TRUE(hg_effects_report_applied(state.generation, false, true, false));
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_FALSE(state.pause_indoor);
    hg_effects_set_pause_indoor_for_leds(false);
}

void test_timing_percentiles(void) {
    hg_timing_stats_t stats = {};
    TEST_ASSERT_EQUAL_UINT32(0, hg_timing_percentile_us(&stats, 95));
    for (unsigned i = 1; i <= 100; ++i) {
        hg_timing_record(&stats, i * 1000U);
    }

    TEST_ASSERT_EQUAL_UINT32(50000, hg_timing_percentile_us(&stats, 50));
    TEST_ASSERT_EQUAL_UINT32(100000, hg_timing_percentile_us(&stats, 95));
    TEST_ASSERT_EQUAL_UINT32(100000, stats.max_us);
}

void test_alarm_cue_replacement_and_precedence(void) {
    hg_effects_init();
    hg_effects_set_capabilities(true, false);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_UNAVAILABLE,
                      hg_effects_request_feedback(HG_FEEDBACK_ALARM_ENTER, 0));
    hg_effects_set_capabilities(true, true);
    uint32_t start = UINT32_MAX - 100U;
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ACCEPTED,
                      hg_effects_request_feedback(HG_FEEDBACK_NAVIGATE, start));
    hg_effects_snapshot_t state;
    for (unsigned i = HG_FEEDBACK_ALARM_ENTER; i <= HG_FEEDBACK_ALARM_DISABLE; ++i) {
        hg_effects_get_snapshot(&state);
        uint32_t previous = state.generation;
        TEST_ASSERT_EQUAL(HG_FEEDBACK_ACCEPTED,
                          hg_effects_request_feedback((hg_feedback_t)i, ++start));
        hg_effects_get_snapshot(&state);
        TEST_ASSERT_EQUAL_UINT32(previous + 1U, state.generation);
        TEST_ASSERT_EQUAL(i, state.feedback);
    }
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ACCEPTED,
                      hg_effects_request_feedback(HG_FEEDBACK_ALARM_DISABLE, ++start));
    hg_effects_get_snapshot(&state);
    uint32_t generation = state.generation;
    TEST_ASSERT_EQUAL(HG_FEEDBACK_RATE_LIMITED,
                      hg_effects_request_feedback(HG_FEEDBACK_ALARM_EDIT, start + 100U));
    TEST_ASSERT_EQUAL(HG_FEEDBACK_RATE_LIMITED,
                      hg_effects_request_feedback(HG_FEEDBACK_NAVIGATE, start + 101U));
    hg_effects_update(start + HG_CUE_FEEDBACK_DURATION_MS - 1U);
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_EQUAL_UINT32(generation, state.generation);
    hg_effects_update(start + HG_CUE_FEEDBACK_DURATION_MS);
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_TRUE(state.stop_pending);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_PREEMPTED,
                      hg_effects_request_feedback(HG_FEEDBACK_ALARM_SAVE,
                                                   start + HG_CUE_FEEDBACK_DURATION_MS));
    TEST_ASSERT_TRUE(hg_effects_report_applied(state.generation, false, false, false));
    TEST_ASSERT_EQUAL(HG_FEEDBACK_ACCEPTED,
                      hg_effects_request_feedback(HG_FEEDBACK_ALARM_SAVE,
                                                   start + HG_CUE_FEEDBACK_DURATION_MS + 1U));
    hg_effects_set_alarm(false, true);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_PREEMPTED,
                      hg_effects_request_feedback(HG_FEEDBACK_ALARM_ENABLE,
                                                   start + HG_CUE_FEEDBACK_DURATION_MS + 2U));
    hg_effects_set_alarm(false, false);
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, state.feedback);
    TEST_ASSERT_TRUE(hg_effects_report_applied(state.generation, false, false, false));
    hg_effects_update(start + 2000U);
    hg_effects_get_snapshot(&state);
    TEST_ASSERT_EQUAL(HG_FEEDBACK_NONE, state.feedback);
}
