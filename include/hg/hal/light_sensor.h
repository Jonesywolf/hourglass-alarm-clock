#ifndef HG_HAL_LIGHT_SENSOR_H
#define HG_HAL_LIGHT_SENSOR_H

#include <stdbool.h>

bool hg_light_sensor_init(void);
bool hg_light_sensor_read_lux(float *lux);

#endif
