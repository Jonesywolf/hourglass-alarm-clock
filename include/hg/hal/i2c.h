#ifndef HG_HAL_I2C_H
#define HG_HAL_I2C_H

#include <stdbool.h>

/* Called only by the sensor worker; all users of Wire share that owner. */
bool hg_i2c_init(void);

#endif
