#include "hg/services/indoor_air.h"

#include "hg/hal/sen66.h"
#include "hg/services/air_rating.h"
#include <Arduino.h>

static hg_indoor_snapshot_t snapshot;
static bool sensor_ready = false;

bool hg_indoor_air_init(void) {
    snapshot = {};
    sensor_ready = hg_sen66_init();
    snapshot.status = sensor_ready ? HG_SAMPLE_WAITING : HG_SAMPLE_UNAVAILABLE;
    return sensor_ready;
}

void hg_indoor_air_update(void) {
    hg_sen66_sample_t sample;
    if (!sensor_ready) {
        return;
    }
    if (!hg_sen66_read(&sample) || !sample.valid) {
        snapshot.status = HG_SAMPLE_ERROR;
        return;
    }

    snapshot.valid = true;
    snapshot.temp_c = sample.temp_c;
    snapshot.humidity = sample.humidity;
    snapshot.pm1_0 = sample.pm1_0;
    snapshot.pm2_5 = sample.pm2_5;
    snapshot.pm4_0 = sample.pm4_0;
    snapshot.pm10 = sample.pm10;
    snapshot.co2_ppm = sample.co2_ppm;
    snapshot.voc_index = sample.voc_index;
    snapshot.nox_index = sample.nox_index;
    hg_air_rate_indoor(&snapshot);
    snapshot.sampled_ms = millis();
    snapshot.status = HG_SAMPLE_READY;
}

void hg_indoor_air_get_snapshot(hg_indoor_snapshot_t *out) {
    if (out) {
        *out = snapshot;
    }
}