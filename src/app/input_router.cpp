#include "hg/app/input_router.h"

#include "hg/hal/encoder.h"
#include "hg/hal/touch.h"
#include "hg/log.h"
#include "hg/services/alarm.h"
#include "hg/types.h"
#include "hg/ui/clock_screen.h"
#include "hg/ui/navigator.h"

static void hg_input_router_apply_encoder(int32_t delta) {
    if (delta == 0) {
        return;
    }

    hg_alarm_snapshot_t alarm;
    hg_alarm_get_snapshot(&alarm);
    if (!alarm.enabled) {
        return;
    }

    int total_minutes = alarm.hour * 60 + alarm.minute + (int)delta;
    total_minutes %= (24 * 60);
    if (total_minutes < 0) {
        total_minutes += 24 * 60;
    }

    hg_alarm_set_time(total_minutes / 60, total_minutes % 60);

    /* Refresh immediately instead of waiting for the next periodic UI tick. */
    hg_alarm_get_snapshot(&alarm);
    hg_clock_screen_update_alarm(&alarm);
}

void hg_input_router_init(void) {
    hg_touch_init();
    hg_encoder_init();
}

void hg_input_router_update(void) {
    hg_touch_update();
    hg_encoder_update();

    if (hg_touch_tapped()) {
        hg_alarm_snapshot_t alarm;
        hg_alarm_get_snapshot(&alarm);
        HG_LOG("input", "tap ringing=%d page=%d\n", alarm.ringing ? 1 : 0,
               (int)hg_navigator_current());
        if (alarm.ringing) {
            hg_alarm_dismiss();
        } else if (hg_navigator_current() == HG_PAGE_CLOCK) {
            hg_navigator_show_page(HG_PAGE_WEATHER);
        } else {
            hg_navigator_show_page(HG_PAGE_CLOCK);
        }
    }

    if (hg_navigator_is_sliding()) {
        return;
    }

    if (hg_encoder_button_pressed()) {
        hg_alarm_snapshot_t alarm;
        hg_alarm_get_snapshot(&alarm);
        hg_alarm_set_enabled(!alarm.enabled);
        hg_alarm_get_snapshot(&alarm);
        hg_clock_screen_update_alarm(&alarm);
    }

    hg_input_router_apply_encoder(hg_encoder_take_delta());
}
