#include "hg/services/indoor_air.h"

#include "hg/hal/sen66.h"

static hg_indoor_snapshot_t snapshot;

void hg_indoor_air_init(void) {
    snapshot.valid = false;
    snapshot.temp_c = 0.0f;
    snapshot.humidity = 0.0f;
    snapshot.pm2_5 = 0.0f;
    snapshot.aqi = 0;
    hg_sen66_init();
}

void hg_indoor_air_update(void) {
    hg_sen66_sample_t sample;
    if (!hg_sen66_read(&sample) || !sample.valid) {
        return;
    }

    snapshot.valid = true;
    snapshot.temp_c = sample.temp_c;
    snapshot.humidity = sample.humidity;
    snapshot.pm2_5 = sample.pm2_5;
    snapshot.aqi = 0;
}

void hg_indoor_air_get_snapshot(hg_indoor_snapshot_t *out) {
    if (out) {
        *out = snapshot;
    }
}
