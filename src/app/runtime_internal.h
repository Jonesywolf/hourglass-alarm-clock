#ifndef HG_APP_RUNTIME_INTERNAL_H
#define HG_APP_RUNTIME_INTERNAL_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "hg/app/runtime.h"

typedef struct {
    uint32_t input_generation;
    uint32_t sensor_generation;
    bool navigation_allowed;
    bool indoor_paused;
} hg_runtime_controls_t;

extern QueueHandle_t hg_input_queue;
extern QueueHandle_t hg_sensor_mailbox;
extern QueueHandle_t hg_network_mailbox;
extern QueueHandle_t hg_audio_command_mailbox;
extern QueueHandle_t hg_audio_status_mailbox;

void hg_runtime_wait_start(void);
hg_runtime_controls_t hg_runtime_controls(void);
bool hg_runtime_publish_input(const hg_input_event_t *event);
void hg_runtime_capture_input_state(const hg_input_state_t *state);
void hg_runtime_record_capture(uint32_t gap_us, bool touch, bool encoder);
void hg_runtime_record_worker(unsigned index, uint32_t elapsed_us);
void hg_runtime_record_audio(uint32_t underruns, uint32_t write_errors);
void hg_runtime_input_fault(void);
void hg_input_capture_task(void *);
void hg_sensor_worker_task(void *);
void hg_network_worker_task(void *);
void hg_audio_worker_task(void *);

inline void hg_worker_delay(TickType_t *last, uint32_t period_ms) {
    TickType_t period = pdMS_TO_TICKS(period_ms);
    configASSERT(period > 0);
    TickType_t now = xTaskGetTickCount();
    if (now - *last >= period) {
        *last = now;
    }
    vTaskDelayUntil(last, period);
}

#ifdef HG_RUNTIME_TEST
void hg_runtime_inject_load(bool sensor);
bool hg_runtime_manual_input(void);
#endif

#endif
