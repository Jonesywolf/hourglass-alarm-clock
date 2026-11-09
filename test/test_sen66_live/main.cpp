#include <Arduino.h>
#include <math.h>
#include <unity.h>

#include "hg/app.h"
#include "hg/app/runtime.h"
#include "hg/hal/light_sensor.h"
#include "hg/hal/sen66.h"
#include "hg/log.h"
#include "hg/services/alarm.h"
#include "hg/services/store.h"

static void pump(uint32_t duration_ms) {
    uint32_t started = millis();
    do {
        hg_app_loop();
    } while (millis() - started < duration_ms);
}

static bool all_channels(const hg_indoor_snapshot_t &indoor) {
    return indoor.valid && indoor.status == HG_SAMPLE_READY &&
           isfinite(indoor.pm1_0) && isfinite(indoor.pm2_5) &&
           isfinite(indoor.pm4_0) && isfinite(indoor.pm10) &&
           isfinite(indoor.temp_c) && isfinite(indoor.humidity) &&
           indoor.co2_ppm > 0 && indoor.voc_index > 0 && indoor.voc_index <= 500 &&
           indoor.nox_index > 0 && indoor.nox_index <= 500;
}

static bool wait_indoor(hg_sample_status_t status, uint32_t previous_ms,
                        uint32_t timeout_ms, hg_sensor_snapshot_t *out) {
    uint32_t started = millis();
    do {
        hg_app_loop();
        hg_runtime_get_sensors(out);
        if (out->indoor.status == status &&
            (status != HG_SAMPLE_READY || out->indoor.sampled_ms != previous_ms)) {
            return true;
        }
    } while (millis() - started < timeout_ms);
    return false;
}

static bool wait_light(hg_sample_status_t status, uint32_t timeout_ms,
                       hg_sensor_snapshot_t *out) {
    uint32_t started = millis();
    do {
        hg_app_loop();
        hg_runtime_get_sensors(out);
        if (out->light.status == status) {
            return true;
        }
    } while (millis() - started < timeout_ms);
    return false;
}

void test_live_sen66_all_channels_and_light(void) {
    TEST_ASSERT_NULL(hg_runtime_fault());
    hg_sensor_snapshot_t observed = {};
    uint32_t started = millis();
    uint32_t first_complete_ms = 0;
    bool first_complete = false;
    bool second_complete = false;
    do {
        hg_app_loop();
        hg_runtime_get_sensors(&observed);
        if (all_channels(observed.indoor) && observed.light.valid &&
            observed.light.status == HG_SAMPLE_READY) {
            if (!first_complete) {
                first_complete = true;
                first_complete_ms = observed.indoor.sampled_ms;
            } else if (observed.indoor.sampled_ms != first_complete_ms) {
                second_complete = true;
                break;
            }
        }
    } while (millis() - started < 75000U);

    Serial.printf("SEN66 status=%d valid=%d PM1=%.1f PM2.5=%.1f PM4=%.1f PM10=%.1f "
                  "temp=%.2f humidity=%.2f CO2=%u VOC=%u NOx=%u level=%d sampled=%lu\n",
                  observed.indoor.status, observed.indoor.valid, observed.indoor.pm1_0,
                  observed.indoor.pm2_5, observed.indoor.pm4_0, observed.indoor.pm10,
                  observed.indoor.temp_c, observed.indoor.humidity, observed.indoor.co2_ppm,
                  observed.indoor.voc_index, observed.indoor.nox_index, observed.indoor.level,
                  (unsigned long)observed.indoor.sampled_ms);
    Serial.printf("LIGHT status=%d valid=%d lux=%.2f sampled=%lu\n",
                  observed.light.status, observed.light.valid, observed.light.lux,
                  (unsigned long)observed.light.sampled_ms);
    TEST_ASSERT_TRUE_MESSAGE(first_complete, "SEN66 or light sensor did not report all channels");
    TEST_ASSERT_TRUE_MESSAGE(second_complete, "SEN66 did not deliver a second fresh sample");
    TEST_ASSERT_TRUE(all_channels(observed.indoor));
    TEST_ASSERT_TRUE(observed.light.valid);
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, observed.light.status);
    TEST_ASSERT_TRUE(isfinite(observed.light.lux));
    TEST_ASSERT_NOT_EQUAL(HG_AIR_UNKNOWN, observed.indoor.level);
}

void test_live_pause_discards_old_acquisition(void) {
    hg_sensor_snapshot_t before;
    hg_runtime_get_sensors(&before);
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, before.indoor.status);
    uint32_t generation = hg_runtime_set_indoor_paused(true);
    uint32_t started = millis();
    hg_sensor_snapshot_t paused = {};
    do {
        delay(10);
        hg_runtime_get_sensors(&paused);
    } while (paused.policy_generation != generation && millis() - started < 2000U);
    TEST_ASSERT_EQUAL_UINT32(generation, paused.policy_generation);
    TEST_ASSERT_EQUAL(HG_SAMPLE_PAUSED, paused.indoor.status);
    uint32_t held_sample = paused.indoor.sampled_ms;
    uint32_t light_sample = paused.light.sampled_ms;
    delay(1500);
    hg_runtime_get_sensors(&paused);
    TEST_ASSERT_EQUAL(HG_SAMPLE_PAUSED, paused.indoor.status);
    TEST_ASSERT_EQUAL_UINT32(held_sample, paused.indoor.sampled_ms);
    TEST_ASSERT_NOT_EQUAL(light_sample, paused.light.sampled_ms);
    hg_runtime_set_indoor_paused(false);
    hg_runtime_get_sensors(&paused);
    TEST_ASSERT_EQUAL(HG_SAMPLE_STALE, paused.indoor.status);
    TEST_ASSERT_TRUE_MESSAGE(wait_indoor(HG_SAMPLE_READY, held_sample, 5000U, &paused),
                             "Indoor sensor did not resume with a fresh sample");
    TEST_ASSERT_TRUE(all_channels(paused.indoor));
}

void test_live_sen66_error_keeps_light_working(void) {
    hg_sensor_snapshot_t observed;
    hg_sen66_test_force_error(true);
    TEST_ASSERT_TRUE_MESSAGE(wait_indoor(HG_SAMPLE_ERROR, 0, 4000U, &observed),
                             "Injected SEN66 error was not surfaced");
    TEST_ASSERT_TRUE(observed.indoor.valid);
    uint32_t retained_sample = observed.indoor.sampled_ms;
    uint32_t light_sample = observed.light.sampled_ms;
    pump(700U);
    hg_runtime_get_sensors(&observed);
    TEST_ASSERT_EQUAL(HG_SAMPLE_ERROR, observed.indoor.status);
    TEST_ASSERT_EQUAL_UINT32(retained_sample, observed.indoor.sampled_ms);
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, observed.light.status);
    TEST_ASSERT_NOT_EQUAL(light_sample, observed.light.sampled_ms);
    hg_sen66_test_force_error(false);
    TEST_ASSERT_TRUE_MESSAGE(wait_indoor(HG_SAMPLE_READY, retained_sample, 10000U, &observed),
                             "SEN66 did not recover after the injected error");
    TEST_ASSERT_TRUE(all_channels(observed.indoor));
}

void test_live_light_error_keeps_sen66_working(void) {
    hg_sensor_snapshot_t observed;
    hg_light_sensor_test_force_error(true);
    TEST_ASSERT_TRUE_MESSAGE(wait_light(HG_SAMPLE_ERROR, 3000U, &observed),
                             "Injected light error was not surfaced");
    uint32_t indoor_sample = observed.indoor.sampled_ms;
    pump(2200U);
    hg_runtime_get_sensors(&observed);
    TEST_ASSERT_EQUAL(HG_SAMPLE_ERROR, observed.light.status);
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, observed.indoor.status);
    TEST_ASSERT_NOT_EQUAL(indoor_sample, observed.indoor.sampled_ms);
    hg_light_sensor_test_force_error(false);
    TEST_ASSERT_TRUE_MESSAGE(wait_light(HG_SAMPLE_READY, 8000U, &observed),
                             "Light sensor did not recover after the injected error");
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, observed.indoor.status);
}

void setUp(void) {}
void tearDown(void) {
    hg_sen66_test_force_error(false);
    hg_light_sensor_test_force_error(false);
    hg_runtime_set_indoor_paused(false);
}

void setup(void) {
    Serial.begin(115200);
    delay(1500);
    hg_log_init();
    hg_store_test_writer(0, false);
    hg_app_setup();
    hg_alarm_set_enabled(false);
    hg_app_test_hide_boot_overlay(true);
    UNITY_BEGIN();
    RUN_TEST(test_live_sen66_all_channels_and_light);
    RUN_TEST(test_live_pause_discards_old_acquisition);
    RUN_TEST(test_live_sen66_error_keeps_light_working);
    RUN_TEST(test_live_light_error_keeps_sen66_working);
    UNITY_END();
}

void loop(void) {
    delay(1000);
}
