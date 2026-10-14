#include <Arduino.h>

#include "hg/app.h"

#ifndef PIO_UNIT_TESTING
void setup() {
    Serial.begin(115200);
    hg_app_setup();
}

void loop() {
    hg_app_loop();
}
#endif
