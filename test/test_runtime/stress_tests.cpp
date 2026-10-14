#include <Arduino.h>
#include <unity.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "hg/app.h"
#include "hg/app/runtime.h"
#include "hg/config.h"
#include "hg/hal/display.h"
#include "hg/services/alarm.h"
#include "hg/services/store.h"
#include "hg/ui/navigator.h"

static StaticSemaphore_t cpu_done_state;
static SemaphoreHandle_t cpu_done = nullptr;
static TaskHandle_t cpu_task = nullptr;
static volatile uint32_t cpu_sink = 1;
static uint32_t cpu_slices = 0;
static bool cpu_load_ok = true;

static void synthetic_output(void *) {
    for (;;) {
        if (ulTaskNotifyTake(pdTRUE, 0)) {
            xSemaphoreGive(cpu_done);
            vTaskDelete(nullptr);
        }
        uint32_t started = micros();
        while (micros() - started < 20000U) {
            cpu_sink = (cpu_sink << 1) ^ (cpu_sink >> 7) ^ 0x1021U;
        }
        ++cpu_slices;
        vTaskDelay(1);
    }
}

static bool start_cpu_load(void) {
    if (!cpu_done) {
        cpu_done = xSemaphoreCreateBinaryStatic(&cpu_done_state);
    }
    cpu_slices = 0;
    return cpu_done && xTaskCreatePinnedToCore(synthetic_output, "test-output", 2048,
                                              nullptr, 1, &cpu_task, 0) == pdPASS;
}

static bool stop_cpu_load(void) {
    if (!cpu_task) {
        return false;
    }
    xTaskNotifyGive(cpu_task);
    bool stopped = xSemaphoreTake(cpu_done, pdMS_TO_TICKS(1000)) == pdTRUE;
    cpu_task = nullptr;
    return stopped && cpu_slices > 10;
}

static void transitions(bool loaded, hg_timing_stats_t *frames,
                         hg_runtime_diagnostics_t *diagnostics) {
    if (loaded) {
        cpu_load_ok = start_cpu_load();
    }
    hg_runtime_test_load(loaded ? 250 : 0, loaded ? 250 : 0, loaded ? 20 : 0);
    uint32_t warmup = millis();
    while (millis() - warmup < 600U) {
        hg_app_loop();
    }
    hg_display_reset_frame_timing();
    hg_runtime_reset_diagnostics();
    for (unsigned i = 0; i < 12; ++i) {
        hg_navigator_next();
        uint32_t started = millis();
        while (hg_navigator_is_sliding() && millis() - started < 2000U) {
            if (loaded) {
                hg_alarm_snapshot_t value = {true, false, 11, (int)(millis() % 60U)};
                hg_store_request_save_alarm(&value);
            }
            hg_app_loop();
        }
        hg_app_loop();
    }
    hg_display_get_frame_timing(frames);
    hg_runtime_get_diagnostics(diagnostics);
    hg_runtime_test_load(0, 0, 0);
    if (loaded && cpu_load_ok) {
        cpu_load_ok = stop_cpu_load();
    }
}

void test_combined_load_frame_timing(void) {
    hg_alarm_set_enabled(false);
    hg_app_test_hide_boot_overlay(true);
    hg_timing_stats_t baseline, loaded;
    hg_runtime_diagnostics_t baseline_diagnostics, loaded_diagnostics;
    transitions(false, &baseline, &baseline_diagnostics);
    hg_display_transfer_timing_t transfer;
    hg_display_get_transfer_timing(&transfer);
    transitions(true, &loaded, &loaded_diagnostics);
    uint32_t baseline_p95 = hg_timing_percentile_us(&baseline, 95);
    uint32_t loaded_p95 = hg_timing_percentile_us(&loaded, 95);
    Serial.printf("FRAME baseline: n=%lu p95=%luus max=%luus; loaded: n=%lu p95=%luus max=%luus\n",
                  (unsigned long)baseline.samples, (unsigned long)baseline_p95,
                  (unsigned long)baseline.max_us, (unsigned long)loaded.samples,
                  (unsigned long)loaded_p95, (unsigned long)loaded.max_us);
    Serial.printf("CAPTURE baseline max=%luus loaded max=%luus; worker max sensor=%luus network=%luus\n",
                  (unsigned long)baseline_diagnostics.capture_gap.max_us,
                  (unsigned long)loaded_diagnostics.capture_gap.max_us,
                  (unsigned long)loaded_diagnostics.sensor_max_us,
                  (unsigned long)loaded_diagnostics.network_max_us);
    Serial.printf("STACK input=%lu sensor=%lu network=%lu bytes free\n",
                  (unsigned long)loaded_diagnostics.stack_free_bytes[0],
                  (unsigned long)loaded_diagnostics.stack_free_bytes[1],
                  (unsigned long)loaded_diagnostics.stack_free_bytes[2]);
    Serial.printf("LVGL baseline work p95=%lu max=%lu gap max=%lu; loaded work p95=%lu max=%lu gap max=%lu\n",
                  (unsigned long)hg_timing_percentile_us(&baseline_diagnostics.ui_work, 95),
                  (unsigned long)baseline_diagnostics.ui_work.max_us,
                  (unsigned long)baseline_diagnostics.ui_gap.max_us,
                  (unsigned long)hg_timing_percentile_us(&loaded_diagnostics.ui_work, 95),
                  (unsigned long)loaded_diagnostics.ui_work.max_us,
                  (unsigned long)loaded_diagnostics.ui_gap.max_us);
    Serial.printf("DISPLAY baseline max transfer=%luus max pixels/frame=%lu\n",
                  (unsigned long)transfer.max_frame_transfer_us,
                  (unsigned long)transfer.max_frame_pixels);
    TEST_ASSERT_GREATER_THAN_UINT32(24, baseline.samples);
    TEST_ASSERT_GREATER_THAN_UINT32(24, loaded.samples);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(50000, baseline.max_us);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(50000, loaded.max_us);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(baseline_p95 + 5000, loaded_p95);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(10000, loaded_diagnostics.capture_gap.max_us);
    TEST_ASSERT_EQUAL_UINT32(0, loaded_diagnostics.input_overflows);
    TEST_ASSERT_GREATER_THAN_UINT32(200000, loaded_diagnostics.sensor_max_us);
    TEST_ASSERT_GREATER_THAN_UINT32(200000, loaded_diagnostics.network_max_us);
    TEST_ASSERT_TRUE(cpu_load_ok);
}

void test_real_nvs_admission_timing(void) {
    hg_store_test_writer(0, false);
    hg_store_set_flash_allowed(true);
    hg_store_status_t status;
    uint32_t started = millis();
    do {
        delay(5);
        hg_store_get_status(&status);
    } while (status.pending && millis() - started < 3000U);
    hg_store_set_flash_allowed(false);
    TEST_ASSERT_FALSE(status.pending);
    Preferences scratch;
    TEST_ASSERT_TRUE(scratch.begin("hg-rt-test", false));
    bool occupied = scratch.isKey("probe");
    scratch.end();
    TEST_ASSERT_FALSE_MESSAGE(occupied, "Scratch key already exists; refusing to overwrite it");

    hg_store_test_scratch(true);
    bool cpu_started = start_cpu_load();
    hg_runtime_test_load(250, 250, 20);
    hg_runtime_reset_diagnostics();
    hg_display_reset_frame_timing();
    uint32_t before_saves = status.successful_saves;
    uint32_t longest_write_us = 0;
    bool saved_each = true;
    for (unsigned i = 0; i < 4; ++i) {
        hg_alarm_snapshot_t value = {false, false, 12, (int)i};
        hg_store_request_save_alarm(&value);
        hg_store_get_status(&status);
        uint32_t target = status.requested_generation;
        hg_navigator_next();
        started = millis();
        while (millis() - started < 1500U) {
            hg_app_loop();
            hg_store_get_status(&status);
            if (!hg_navigator_is_sliding() && status.saved_generation == target) {
                break;
            }
        }
        saved_each = saved_each && status.saved_generation == target;
        if (status.last_write_us > longest_write_us) {
            longest_write_us = status.last_write_us;
        }
    }
    hg_runtime_test_load(0, 0, 0);
    bool cpu_stopped = cpu_started && stop_cpu_load();
    hg_store_set_flash_allowed(false);
    hg_runtime_diagnostics_t diagnostics;
    hg_runtime_get_diagnostics(&diagnostics);
    hg_timing_stats_t frames;
    hg_display_get_frame_timing(&frames);
    hg_store_test_scratch(false);
    bool removed = false;
    if (!status.writing && scratch.begin("hg-rt-test", false)) {
        removed = scratch.remove("probe");
        scratch.end();
    }
    Serial.printf("NVS scratch saves=%lu longest=%luus capture max=%luus frame max=%luus\n",
                  (unsigned long)(status.successful_saves - before_saves),
                  (unsigned long)longest_write_us,
                  (unsigned long)diagnostics.capture_gap.max_us,
                  (unsigned long)frames.max_us);
    TEST_ASSERT_TRUE(removed);
    TEST_ASSERT_TRUE(cpu_stopped);
    TEST_ASSERT_TRUE(saved_each);
    TEST_ASSERT_EQUAL_UINT32(4, status.successful_saves - before_saves);
    TEST_ASSERT_GREATER_THAN_UINT32(0, longest_write_us);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(10000, diagnostics.capture_gap.max_us);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(50000, frames.max_us);
}
