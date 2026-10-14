#ifndef HG_HAL_LIGHT_SENSOR_H
#define HG_HAL_LIGHT_SENSOR_H

#include <stdbool.h>
#include "hg/types.h"

bool hg_light_sensor_init(void);
hg_sample_status_t hg_light_sensor_read_lux(float *lux);

#endif
