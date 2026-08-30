#include "hg/services/time.h"

#include <Arduino.h>
#include <time.h>

#include "hg/config.h"
#include "hg/hal/rtc.h"

void hg_time_init(void) {
    configTime(HG_GMT_OFFSET_SEC, HG_DAYLIGHT_OFFSET_SEC, HG_NTP_SERVER);
}

void hg_time_get_snapshot(hg_time_snapshot_t *out) {
    if (!out) {
        return;
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, HG_TIME_QUERY_TIMEOUT_MS)) {
        if (!hg_rtc_get_time(out)) {
            out->valid = false;
            out->hour_24 = 0;
            out->minute = 0;
            out->second = 0;
        }
        return;
    }

    out->valid = true;
    out->hour_24 = timeinfo.tm_hour;
    out->minute = timeinfo.tm_min;
    out->second = timeinfo.tm_sec;
    (void)hg_rtc_set_time(out);
}
