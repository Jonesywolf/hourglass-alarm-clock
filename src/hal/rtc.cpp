#include "hg/hal/rtc.h"

#include <Arduino.h>
#include <time.h>

static bool rtc_has_base = false;
static time_t rtc_base_epoch = 0;
static uint32_t rtc_base_ms = 0;

static bool snapshot_to_epoch(const hg_time_snapshot_t *in, time_t *out_epoch) {
    if (!in || !out_epoch || !in->valid) {
        return false;
    }

    struct tm local_tm;
    local_tm.tm_year = 124;
    local_tm.tm_mon = 0;
    local_tm.tm_mday = 1;
    local_tm.tm_hour = in->hour_24;
    local_tm.tm_min = in->minute;
    local_tm.tm_sec = in->second;
    local_tm.tm_isdst = -1;

    time_t epoch = mktime(&local_tm);
    if (epoch < 0) {
        return false;
    }

    *out_epoch = epoch;
    return true;
}

static bool epoch_to_snapshot(time_t epoch, hg_time_snapshot_t *out) {
    if (!out) {
        return false;
    }

    struct tm local_tm;
    if (!localtime_r(&epoch, &local_tm)) {
        out->valid = false;
        out->hour_24 = 0;
        out->minute = 0;
        out->second = 0;
        return false;
    }

    out->valid = true;
    out->hour_24 = local_tm.tm_hour;
    out->minute = local_tm.tm_min;
    out->second = local_tm.tm_sec;
    return true;
}

bool hg_rtc_init(void) {
    rtc_has_base = false;
    rtc_base_epoch = 0;
    rtc_base_ms = millis();
    return true;
}

bool hg_rtc_get_time(hg_time_snapshot_t *out) {
    if (!out || !rtc_has_base) {
        if (out) {
            out->valid = false;
            out->hour_24 = 0;
            out->minute = 0;
            out->second = 0;
        }
        return false;
    }

    uint32_t elapsed_ms = millis() - rtc_base_ms;
    time_t epoch = rtc_base_epoch + (time_t)(elapsed_ms / 1000U);
    return epoch_to_snapshot(epoch, out);
}

bool hg_rtc_set_time(const hg_time_snapshot_t *in) {
    time_t epoch;
    if (!snapshot_to_epoch(in, &epoch)) {
        return false;
    }

    rtc_base_epoch = epoch;
    rtc_base_ms = millis();
    rtc_has_base = true;
    return true;
}
