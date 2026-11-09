#include "hg/services/indoor_air.h"

#include "hg/hal/sen66.h"
#include "hg/services/air_rating.h"

static hg_indoor_snapshot_t snapshot;

void hg_indoor_air_init(void) {
    snapshot = {};
    snapshot.status = HG_SAMPLE_WAITING;
    hg_sen66_init();
}

void hg_indoor_air_set_paused(bool paused, uint32_t now_ms) {
    hg_sen66_set_paused(paused, now_ms);
    snapshot.status = paused ? HG_SAMPLE_PAUSED :
                      (snapshot.valid ? HG_SAMPLE_STALE : HG_SAMPLE_WAITING);
}

void hg_indoor_air_update(uint32_t now_ms) {
    hg_sen66_sample_t sample = {};
    hg_sample_status_t status = hg_sen66_update(now_ms, &sample);
    if (status == HG_SAMPLE_WAITING) {
        if (snapshot.status == HG_SAMPLE_ERROR || snapshot.status == HG_SAMPLE_UNAVAILABLE) {
            snapshot.status = snapshot.valid ? HG_SAMPLE_STALE : HG_SAMPLE_WAITING;
        }
        return;
    }
    if (status != HG_SAMPLE_READY) {
        snapshot.status = status;
        return;
    }
    if (!sample.valid) {
        snapshot.status = snapshot.valid ? HG_SAMPLE_STALE : HG_SAMPLE_WAITING;
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
    snapshot.sampled_ms = now_ms;
    snapshot.status = HG_SAMPLE_READY;
}

void hg_indoor_air_get_snapshot(hg_indoor_snapshot_t *out) {
    if (out) {
        *out = snapshot;
    }
}