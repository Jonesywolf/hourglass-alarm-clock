#include "hg/services/air_rating.h"

#include <math.h>

#include "hg/config.h"

static hg_air_level_t rate(float value, float fair, float poor) {
    if (value >= poor) {
        return HG_AIR_POOR;
    }
    return value >= fair ? HG_AIR_FAIR : HG_AIR_GOOD;
}

static void consider(hg_indoor_snapshot_t *indoor, hg_air_level_t level, hg_air_cause_t cause) {
    if (level > indoor->level) {
        indoor->level = level;
        indoor->cause = cause;
    } else if (indoor->level == HG_AIR_UNKNOWN) {
        indoor->level = level;
    }
}

void hg_air_rate_indoor(hg_indoor_snapshot_t *indoor) {
    if (!indoor) {
        return;
    }
    indoor->level = HG_AIR_UNKNOWN;
    indoor->cause = HG_AIR_CAUSE_NONE;
    if (!indoor->valid) {
        return;
    }
    bool have_co2 = indoor->co2_ppm > 0;
    bool have_pm2_5 = isfinite(indoor->pm2_5) && indoor->pm2_5 >= 0.0f;
    bool have_voc = indoor->voc_index > 0 && indoor->voc_index <= 500;
    if (have_co2) {
        consider(indoor, rate(indoor->co2_ppm, HG_INDOOR_CO2_FAIR_PPM, HG_INDOOR_CO2_POOR_PPM),
                 HG_AIR_CAUSE_CO2);
    }
    if (have_pm2_5) {
        consider(indoor, rate(indoor->pm2_5, HG_INDOOR_PM25_FAIR, HG_INDOOR_PM25_POOR),
                 HG_AIR_CAUSE_PM2_5);
    }
    if (have_voc) {
        consider(indoor, rate(indoor->voc_index, HG_INDOOR_VOC_FAIR, HG_INDOOR_VOC_POOR),
                 HG_AIR_CAUSE_VOC);
    }
    if (indoor->level != HG_AIR_POOR && !(have_co2 && have_pm2_5 && have_voc)) {
        indoor->level = HG_AIR_UNKNOWN;
        indoor->cause = HG_AIR_CAUSE_NONE;
    } else if (indoor->level == HG_AIR_GOOD) {
        indoor->cause = HG_AIR_CAUSE_NONE;
    }
}

int hg_air_us_aqi_category(int aqi) {
    static const int limits[] = {50, 100, 150, 200, 300};
    for (int i = 0; i < 5; ++i) {
        if (aqi <= limits[i]) {
            return i;
        }
    }
    return 5;
}