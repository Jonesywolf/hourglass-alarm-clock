#include "hg/hal/i2c.h"

#include <Wire.h>

#include "hg/config.h"
#include "hg/hal/pins.h"

bool hg_i2c_init(void) {
    if (!Wire.begin(HG_PIN_I2C_SDA, HG_PIN_I2C_SCL)) {
        return false;
    }
    Wire.setTimeOut(HG_I2C_TIMEOUT_MS);
    return true;
}
