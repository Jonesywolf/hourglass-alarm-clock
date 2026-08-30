#include <Arduino.h>

#include "hg/app.h"

void setup() {
    Serial.begin(115200);
    hg_app_setup();
}

void loop() {
    hg_app_loop();
}
