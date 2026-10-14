#include "hg/app/runtime.h"

#include <Arduino.h>
#include <freertos/event_groups.h>

#include "hg/config.h"
#include "hg/log.h"
#include "hg/services/store.h"
#include "runtime_internal.h"

QueueHandle_t hg_input_queue = nullptr;
QueueHandle_t hg_sensor_mailbox = nullptr;
QueueHandle_t hg_network_mailbox = nullptr;
static StaticQueue_t input_state, sensor_state, network_state;
static uint8_t input_storage[HG_INPUT_QUEUE_CAPACITY * sizeof(hg_input_event_t)];
static uint8_t sensor_storage[sizeof(hg_sensor_snapshot_t)];
static uint8_t network_storage[sizeof(hg_network_snapshot_t)];
static StaticEventGroup_t start_state;
static EventGroupHandle_t start_event = nullptr;
static TaskHandle_t workers[3] = {};
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static hg_runtime_controls_t controls = {1, 1, false, false};
static hg_runtime_diagnostics_t diagnostics;
static bool running = false;
static const char *startup_fault = nullptr;
static uint32_t last_ui_us = 0;
static bool ui_was_sliding = false;

#ifdef HG_RUNTIME_TEST
static int fail_worker = -1;
static uint32_t sensor_load_ms = 0, network_load_ms = 0, busy_load_ms = 0;

void hg_runtime_test_fail_worker(int index) {
    fail_worker = index;
}

void hg_runtime_test_load(uint32_t sensor_ms, uint32_t network_ms, uint32_t busy_ms) {
    portENTER_CRITICAL(&mux);
    sensor_load_ms = sensor_ms;
    network_load_ms = network_ms;
    busy_load_ms = busy_ms;
    portEXIT_CRITICAL(&mux);
}

void hg_runtime_inject_load(bool sensor) {
    portENTER_CRITICAL(&mux);
    uint32_t sleep_ms = sensor ? sensor_load_ms : network_load_ms;
    uint32_t busy_ms = sensor ? 0 : busy_load_ms;
    portEXIT_CRITICAL(&mux);
    if (sleep_ms) {
        delay(sleep_ms);
    }
    uint32_t start = millis();
    while (millis() - start < busy_ms) {
        asm volatile("nop");
    }
}

bool hg_runtime_test_input(const hg_input_event_t *event) {
    return hg_runtime_publish_input(event);
}

void hg_runtime_test_sensor_cache(const hg_sensor_snapshot_t *snapshot) {
    xQueueOverwrite(hg_sensor_mailbox, snapshot);
}
#endif

bool hg_runtime_start(void) {
    if (running) {
        HG_ERROR("runtime", "Runtime already started\n");
        return false;
    }
    startup_fault = nullptr;
    controls = {1, 1, false, false};
    hg_input_queue = xQueueCreateStatic(HG_INPUT_QUEUE_CAPACITY, sizeof(hg_input_event_t),
                                        input_storage, &input_state);
    hg_sensor_mailbox = xQueueCreateStatic(1, sizeof(hg_sensor_snapshot_t),
                                          sensor_storage, &sensor_state);
    hg_network_mailbox = xQueueCreateStatic(1, sizeof(hg_network_snapshot_t),
                                           network_storage, &network_state);
    start_event = xEventGroupCreateStatic(&start_state);
    if (!hg_input_queue || !hg_sensor_mailbox || !hg_network_mailbox || !start_event) {
        startup_fault = "Runtime queue allocation failed";
        HG_ERROR("runtime", "%s\n", startup_fault);
        return false;
    }
    hg_sensor_snapshot_t sensors = {};
    hg_network_snapshot_t network = {};
    xQueueOverwrite(hg_sensor_mailbox, &sensors);
    xQueueOverwrite(hg_network_mailbox, &network);
    TaskFunction_t entries[] = {hg_input_capture_task, hg_sensor_worker_task,
                               hg_network_worker_task};
    const char *names[] = {"hg-input", "hg-sensors", "hg-network"};
    const uint32_t stacks[] = {3072, 4096, 6144};
    for (unsigned i = 0; i < 4; ++i) {
        bool ok = true;
#ifdef HG_RUNTIME_TEST
        ok = fail_worker != (int)i;
#endif
        if (ok) {
            ok = i == 3 ? hg_store_start_worker() :
                 xTaskCreatePinnedToCore(entries[i], names[i], stacks[i], nullptr,
                                         i == 0 ? 2 : 1, &workers[i],
                                         i == 0 ? ARDUINO_RUNNING_CORE : 0) == pdPASS;
        }
        if (!ok) {
            startup_fault = "Runtime worker creation failed";
            for (TaskHandle_t &worker : workers) {
                if (worker) {
                    vTaskDelete(worker);
                    worker = nullptr;
                }
            }
            HG_ERROR("runtime", "%s (worker %u)\n", startup_fault, i);
            return false;
        }
    }
    running = true;
    xEventGroupSetBits(start_event, 1);
    return true;
}

void hg_runtime_wait_start(void) {
    xEventGroupWaitBits(start_event, 1, pdFALSE, pdTRUE, portMAX_DELAY);
}

const char *hg_runtime_fault(void) {
    if (startup_fault) {
        return startup_fault;
    }
    portENTER_CRITICAL(&mux);
    bool input_fault = diagnostics.input_fault;
    portEXIT_CRITICAL(&mux);
    return input_fault ? "Input hardware initialization failed" : nullptr;
}

hg_runtime_controls_t hg_runtime_controls(void) {
    portENTER_CRITICAL(&mux);
    hg_runtime_controls_t result = controls;
    portEXIT_CRITICAL(&mux);
    return result;
}

void hg_runtime_set_navigation_allowed(bool allowed) {
    portENTER_CRITICAL(&mux);
    if (controls.navigation_allowed != allowed) {
        controls.navigation_allowed = allowed;
        ++controls.input_generation;
    }
    portEXIT_CRITICAL(&mux);
}

uint32_t hg_runtime_input_generation(void) {
    return hg_runtime_controls().input_generation;
}

uint32_t hg_runtime_set_indoor_paused(bool paused) {
    portENTER_CRITICAL(&mux);
    if (controls.indoor_paused != paused) {
        controls.indoor_paused = paused;
        ++controls.sensor_generation;
    }
    uint32_t result = controls.sensor_generation;
    portEXIT_CRITICAL(&mux);
    return result;
}

void hg_runtime_get_sensors(hg_sensor_snapshot_t *out) {
    if (xQueuePeek(hg_sensor_mailbox, out, 0) != pdTRUE) {
        *out = {};
        out->light.status = HG_SAMPLE_ERROR;
        out->indoor.status = HG_SAMPLE_ERROR;
        HG_ERROR("runtime", "Missing sensor cache\n");
        return;
    }
    hg_runtime_controls_t current = hg_runtime_controls();
    uint32_t now = millis();
    if (out->light.valid && out->light.status == HG_SAMPLE_READY &&
        now - out->light.sampled_ms >= HG_SENSOR_STALE_MS) {
        out->light.status = HG_SAMPLE_STALE;
    }
    if (current.indoor_paused) {
        out->indoor.status = HG_SAMPLE_PAUSED;
    } else if (out->indoor.status == HG_SAMPLE_PAUSED ||
               (out->indoor.valid && out->indoor.status == HG_SAMPLE_READY &&
                (out->policy_generation != current.sensor_generation ||
                now - out->indoor.sampled_ms >= HG_SENSOR_STALE_MS))) {
        out->indoor.status = HG_SAMPLE_STALE;
    }
}

void hg_runtime_get_network(hg_network_snapshot_t *out) {
    if (xQueuePeek(hg_network_mailbox, out, 0) != pdTRUE) {
        *out = {};
        out->stale = true;
        HG_ERROR("runtime", "Missing network cache\n");
        return;
    }
    out->stale = !out->ready || millis() - out->updated_ms >= HG_NETWORK_STALE_MS;
}

bool hg_runtime_publish_input(const hg_input_event_t *event) {
    bool sent = xQueueSend(hg_input_queue, event, 0) == pdTRUE;
    uint32_t depth = uxQueueMessagesWaiting(hg_input_queue);
    portENTER_CRITICAL(&mux);
    if (!sent) {
        ++diagnostics.input_overflows;
    }
    if (depth > diagnostics.input_queue_high_water) {
        diagnostics.input_queue_high_water = depth;
    }
    portEXIT_CRITICAL(&mux);
    return sent;
}

bool hg_runtime_take_input(hg_input_event_t *out) {
    if (xQueueReceive(hg_input_queue, out, 0) != pdTRUE) {
        return false;
    }
    uint32_t age = millis() - out->captured_ms;
    portENTER_CRITICAL(&mux);
    hg_timing_record(&diagnostics.input_age, age > UINT32_MAX / 1000U ? UINT32_MAX : age * 1000U);
    if (age > HG_INPUT_MAX_AGE_MS) {
        ++diagnostics.expired_inputs;
    }
    portEXIT_CRITICAL(&mux);
    if (age > HG_INPUT_MAX_AGE_MS) {
        if (out->tapped) {
            HG_TOUCH_LOG("input", "tap expired age=%lu ms\n", (unsigned long)age);
        }
        out->tapped = false;
        out->encoder_pressed = false;
        out->encoder_delta = 0;
    }
    return true;
}

void hg_runtime_record_capture(uint32_t gap_us, bool touch, bool encoder) {
    portENTER_CRITICAL(&mux);
    if (gap_us) {
        hg_timing_record(&diagnostics.capture_gap, gap_us);
    }
    diagnostics.touch_pressed = touch;
    diagnostics.encoder_pressed = encoder;
    portEXIT_CRITICAL(&mux);
}

void hg_runtime_record_worker(unsigned index, uint32_t elapsed_us) {
    uint32_t margin = uxTaskGetStackHighWaterMark(nullptr);
    portENTER_CRITICAL(&mux);
    diagnostics.stack_free_bytes[index] = margin;
    uint32_t *maximum = index == 1 ? &diagnostics.sensor_max_us :
                        index == 2 ? &diagnostics.network_max_us : nullptr;
    if (maximum && elapsed_us > *maximum) {
        *maximum = elapsed_us;
    }
    portEXIT_CRITICAL(&mux);
}

void hg_runtime_input_fault(void) {
    portENTER_CRITICAL(&mux);
    diagnostics.input_fault = true;
    portEXIT_CRITICAL(&mux);
}

void hg_runtime_record_ui(uint32_t started_us, uint32_t elapsed_us, bool sliding) {
    portENTER_CRITICAL(&mux);
    if (sliding) {
        hg_timing_record(&diagnostics.ui_work, elapsed_us);
        if (ui_was_sliding) {
            hg_timing_record(&diagnostics.ui_gap, started_us - last_ui_us);
        }
    }
    last_ui_us = started_us;
    ui_was_sliding = sliding;
    portEXIT_CRITICAL(&mux);
}

void hg_runtime_get_diagnostics(hg_runtime_diagnostics_t *out) {
    portENTER_CRITICAL(&mux);
    *out = diagnostics;
    portEXIT_CRITICAL(&mux);
}

void hg_runtime_reset_diagnostics(void) {
    portENTER_CRITICAL(&mux);
    bool input_fault = diagnostics.input_fault;
    diagnostics = {};
    diagnostics.input_fault = input_fault;
    ui_was_sliding = false;
    portEXIT_CRITICAL(&mux);
}
