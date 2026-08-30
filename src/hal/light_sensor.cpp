#include "hg/hal/light_sensor.h"

bool hg_light_sensor_init(void) {
    return false;
}

bool hg_light_sensor_read_lux(float *lux) {
    if (lux) {
        *lux = 0.0f;
    }
    return false;
}
