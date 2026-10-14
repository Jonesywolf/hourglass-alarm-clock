#include <Arduino.h>

#include "hg/config.h"
#include "hg/hal/i2c.h"
#include "hg/hal/light_sensor.h"
#include "hg/log.h"
#include "hg/services/indoor_air.h"
#include "runtime_internal.h"

void hg_sensor_worker_task(void *) {
    hg_runtime_wait_start();
    hg_sensor_snapshot_t snapshot = {};
    bool bus_ready = false;
    bool light_ready = false;
    bool indoor_initialized = false;
    uint32_t last_retry_ms = millis() - HG_SENSOR_RETRY_MS;
    uint32_t last_light_ms = millis() - HG_LIGHT_SAMPLE_MS;
    uint32_t last_indoor_ms = millis() - HG_INDOOR_SAMPLE_MS;
    uint32_t last_indoor_error_ms = millis() - HG_SENSOR_RETRY_MS;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        uint32_t started_us = micros();
#ifdef HG_RUNTIME_TEST
        hg_runtime_inject_load(true);
#endif
        uint32_t now = millis();
        if (!light_ready && now - last_retry_ms >= HG_SENSOR_RETRY_MS) {
            last_retry_ms = now;
            if (!bus_ready) {
                bus_ready = hg_i2c_init();
            }
            light_ready = bus_ready && hg_light_sensor_init();
            snapshot.light.status = light_ready ? HG_SAMPLE_WAITING : HG_SAMPLE_ERROR;
            if (!light_ready) {
                HG_ERROR("sensor", "Ambient light unavailable; retry scheduled\n");
            }
        }
        if (bus_ready && !indoor_initialized) {
            hg_indoor_air_init();
            hg_indoor_air_get_snapshot(&snapshot.indoor);
            indoor_initialized = true;
        }
        uint32_t light_interval = snapshot.light.status == HG_SAMPLE_WAITING ?
                                  20U : HG_LIGHT_SAMPLE_MS;
        if (light_ready && now - last_light_ms >= light_interval) {
            last_light_ms = now;
            float lux = 0.0f;
            snapshot.light.status = hg_light_sensor_read_lux(&lux);
            if (snapshot.light.status == HG_SAMPLE_READY) {
                snapshot.light.lux = lux;
                snapshot.light.valid = true;
                snapshot.light.sampled_ms = millis();
            } else if (snapshot.light.status == HG_SAMPLE_ERROR) {
                light_ready = false;
                last_retry_ms = millis();
                HG_ERROR("sensor", "Ambient light read failed; last value retained\n");
            }
        }
        hg_runtime_controls_t before = hg_runtime_controls();
        if (before.indoor_paused) {
            snapshot.indoor.status = HG_SAMPLE_PAUSED;
        } else if (indoor_initialized && now - last_indoor_ms >= HG_INDOOR_SAMPLE_MS) {
            last_indoor_ms = now;
            hg_indoor_air_update();
            hg_indoor_snapshot_t sample;
            hg_indoor_air_get_snapshot(&sample);
            hg_runtime_controls_t after = hg_runtime_controls();
            if (after.sensor_generation == before.sensor_generation) {
                snapshot.indoor = sample;
            } else {
                snapshot.indoor.status = after.indoor_paused ? HG_SAMPLE_PAUSED :
                                         HG_SAMPLE_STALE;
            }
            if (sample.status == HG_SAMPLE_ERROR &&
                millis() - last_indoor_error_ms >= HG_SENSOR_RETRY_MS) {
                last_indoor_error_ms = millis();
                HG_ERROR("sensor", "Indoor acquisition failed; last value retained\n");
            }
        }
        hg_runtime_controls_t current = hg_runtime_controls();
        snapshot.policy_generation = current.sensor_generation;
        if (current.indoor_paused) {
            snapshot.indoor.status = HG_SAMPLE_PAUSED;
        } else if (snapshot.indoor.status == HG_SAMPLE_PAUSED) {
            snapshot.indoor.status = snapshot.indoor.valid ? HG_SAMPLE_STALE : HG_SAMPLE_WAITING;
        }
        snapshot.updated_ms = millis();
        xQueueOverwrite(hg_sensor_mailbox, &snapshot);
        hg_runtime_record_worker(1, micros() - started_us);
        hg_worker_delay(&last_wake, 20U);
    }
}
