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
    bool indoor_policy_applied = false;
    uint32_t applied_generation = 0;
    uint32_t last_bus_retry_ms = millis() - HG_SENSOR_RETRY_MS;
    uint32_t last_light_retry_ms = millis() - HG_SENSOR_RETRY_MS;
    uint32_t last_light_ms = millis() - HG_LIGHT_SAMPLE_MS;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        uint32_t started_us = micros();
#ifdef HG_RUNTIME_TEST
        hg_runtime_inject_load(true);
#endif
        uint32_t now = millis();
        if (!bus_ready && now - last_bus_retry_ms >= HG_SENSOR_RETRY_MS) {
            last_bus_retry_ms = now;
            bus_ready = hg_i2c_init();
            if (!bus_ready) {
                snapshot.light.status = HG_SAMPLE_ERROR;
                snapshot.indoor.status = HG_SAMPLE_UNAVAILABLE;
                HG_ERROR("sensor", "I2C unavailable; retry scheduled\n");
            }
        }
        if (bus_ready && !light_ready && now - last_light_retry_ms >= HG_SENSOR_RETRY_MS) {
            last_light_retry_ms = now;
            light_ready = hg_light_sensor_init();
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
                last_light_retry_ms = millis();
                HG_ERROR("sensor", "Ambient light read failed; last value retained\n");
            }
        }
        hg_runtime_controls_t before = hg_runtime_controls();
        if (indoor_initialized) {
            if (!indoor_policy_applied || applied_generation != before.sensor_generation) {
                hg_indoor_air_set_paused(true, now);
                if (!before.indoor_paused) {
                    hg_indoor_air_set_paused(false, now);
                }
                applied_generation = before.sensor_generation;
                indoor_policy_applied = true;
            }
            if (!before.indoor_paused) {
                hg_indoor_air_update(now);
            }
            hg_indoor_snapshot_t sample;
            hg_indoor_air_get_snapshot(&sample);
            hg_runtime_controls_t after = hg_runtime_controls();
            if (after.sensor_generation == before.sensor_generation) {
                snapshot.indoor = sample;
            } else {
                hg_indoor_air_set_paused(true, millis());
                if (!after.indoor_paused) {
                    hg_indoor_air_set_paused(false, millis());
                }
                applied_generation = after.sensor_generation;
                snapshot.indoor.status = after.indoor_paused ? HG_SAMPLE_PAUSED :
                    (snapshot.indoor.valid ? HG_SAMPLE_STALE : HG_SAMPLE_WAITING);
            }
        } else {
            applied_generation = before.sensor_generation;
            snapshot.indoor.status = before.indoor_paused ? HG_SAMPLE_PAUSED :
                                                          HG_SAMPLE_UNAVAILABLE;
        }
        hg_runtime_controls_t current = hg_runtime_controls();
        if (current.sensor_generation != applied_generation && indoor_initialized) {
            hg_indoor_air_set_paused(true, millis());
            if (!current.indoor_paused) {
                hg_indoor_air_set_paused(false, millis());
            }
            applied_generation = current.sensor_generation;
            snapshot.indoor.status = current.indoor_paused ? HG_SAMPLE_PAUSED :
                (snapshot.indoor.valid ? HG_SAMPLE_STALE : HG_SAMPLE_WAITING);
        }
        snapshot.policy_generation = applied_generation;
        snapshot.updated_ms = millis();
        xQueueOverwrite(hg_sensor_mailbox, &snapshot);
        hg_runtime_record_worker(1, micros() - started_us);
        hg_worker_delay(&last_wake, 20U);
    }
}
