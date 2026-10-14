#include "hg/services/indoor_air.h"

#include "hg/hal/sen66.h"
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
    snapshot.pm2_5 = sample.pm2_5;
    snapshot.aqi = 0;
    snapshot.sampled_ms = millis();
    snapshot.status = HG_SAMPLE_READY;
}

void hg_indoor_air_get_snapshot(hg_indoor_snapshot_t *out) {
    if (out) {
        *out = snapshot;
    }
}
