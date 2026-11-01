#include <Arduino.h>

#include "hg/log.h"
#include "hg/services/phone_bridge.h"
#include "hg/services/wifi.h"
#include "runtime_internal.h"

void hg_network_worker_task(void *) {
    hg_runtime_wait_start();
    hg_wifi_init();
    hg_phone_bridge_init();
    hg_network_snapshot_t snapshot = {};
    uint32_t last_update_ms = millis() - 1000U;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        uint32_t started_us = micros();
#ifdef HG_RUNTIME_TEST
        hg_runtime_inject_load(false);
#endif
        if (millis() - last_update_ms >= 1000U) {
            last_update_ms = millis();
            hg_wifi_update();
            hg_phone_bridge_update();
            hg_wifi_get_snapshot(&snapshot.wifi);
            /* Never waits: the weather worker owns blocking HTTPS. */
            xQueuePeek(hg_weather_mailbox, &snapshot.weather, 0);
            snapshot.updated_ms = millis();
            snapshot.ready = true;
            xQueueOverwrite(hg_network_mailbox, &snapshot);
        }
        hg_log_flush();
        hg_runtime_record_worker(2, micros() - started_us);
        hg_worker_delay(&last_wake, 20U);
    }
}
