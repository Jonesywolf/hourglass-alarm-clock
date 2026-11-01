#include <Arduino.h>

#include "hg/services/location.h"
#include "hg/services/weather.h"
#include "runtime_internal.h"

/* Owns all blocking HTTPS (location + Open-Meteo). Publishes the latest weather POD;
 * the network worker merges it into the network snapshot without waiting. */
void hg_weather_worker_task(void *) {
    hg_runtime_wait_start();
    hg_location_init();
    hg_weather_init();
    hg_weather_snapshot_t snapshot;
    uint32_t logged_sampled_ms = 0;
    hg_sample_status_t logged_status = HG_SAMPLE_WAITING;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        hg_runtime_record_worker(4, 0);
        hg_location_update();
        hg_weather_update();
        hg_weather_get_snapshot(&snapshot);
        xQueueOverwrite(hg_weather_mailbox, &snapshot);
        if (snapshot.sampled_ms != logged_sampled_ms || snapshot.status != logged_status) {
            logged_sampled_ms = snapshot.sampled_ms;
            logged_status = snapshot.status;
            // Stack headroom after a real TLS fetch is the margin that matters on device.
            Serial.printf("Weather: status %d valid %d hours %u aqi %d stack free %u B\n",
                          (int)snapshot.status, snapshot.valid ? 1 : 0,
                          (unsigned)snapshot.hourly_count,
                          snapshot.air_valid ? (int)snapshot.us_aqi : -1,
                          (unsigned)uxTaskGetStackHighWaterMark(nullptr));
        }
        hg_runtime_record_worker(4, 0);
        hg_worker_delay(&last_wake, 1000U);
    }
}
