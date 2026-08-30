#include "hg/services/weather.h"

static hg_weather_snapshot_t snapshot;

void hg_weather_init(void) {
    snapshot.valid = false;
    snapshot.temp_c = 0.0f;
    snapshot.weather_code = 0;
    snapshot.aqi = 0;
}

void hg_weather_update(void) {}

void hg_weather_get_snapshot(hg_weather_snapshot_t *out) {
    if (out) {
        *out = snapshot;
    }
}
