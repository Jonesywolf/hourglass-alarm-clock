#include "hg/app/input_router.h"

#include "hg/hal/encoder.h"
#include "hg/hal/touch.h"

void hg_input_router_init(void) {
    hg_touch_init();
    hg_encoder_init();
}

void hg_input_router_update(void) {
    hg_touch_update();
    hg_encoder_update();
    (void)hg_touch_is_pressed();
    (void)hg_encoder_take_delta();
    (void)hg_encoder_button_pressed();
}
