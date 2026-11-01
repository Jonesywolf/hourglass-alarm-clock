#include "hg/services/glance.h"

#include "hg/config.h"

static bool is_storm(hg_weather_condition_t condition) {
    return condition == HG_WX_THUNDERSTORM || condition == HG_WX_HAIL;
}

static void set_line(hg_glance_snapshot_t *out, hg_glance_line_t line, float value) {
    out->line = line;
    out->value = value;
}

void hg_glance_compute(const hg_weather_snapshot_t *weather, const hg_indoor_snapshot_t *indoor,
                       uint32_t now_ms, bool alarm_stopped, uint32_t alarm_stopped_ms,
                       hg_glance_snapshot_t *out) {
    if (!out) {
        return;
    }
    *out = {};
    out->rain_start_hour = -1;
    bool have_weather = weather && weather->valid &&
                        now_ms - weather->sampled_ms < HG_WEATHER_EXPIRED_MS;
    if (have_weather) {
        out->show_weather = true;
        out->muted = weather->status == HG_SAMPLE_STALE;
        out->condition = weather->condition;
        out->is_day = weather->is_day;
        out->temp_c = weather->temp_c;
    }
    bool indoor_poor = indoor && indoor->valid && indoor->status == HG_SAMPLE_READY &&
                       indoor->level == HG_AIR_POOR;
    bool aqi_bad = have_weather && weather->air_valid &&
                   weather->air_status != HG_SAMPLE_STALE &&
                   weather->us_aqi >= HG_GLANCE_OUTDOOR_AQI;
    bool briefing = have_weather && alarm_stopped &&
                    now_ms - alarm_stopped_ms < HG_GLANCE_BRIEFING_MS;

    if (have_weather && (is_storm(weather->condition) || is_storm(weather->day_condition))) {
        set_line(out, HG_GLANCE_LINE_THUNDER, is_storm(weather->condition) ? 1.0f : 0.0f);
    } else if (indoor_poor) {
        out->indoor_cause = indoor->cause;
        float value = indoor->cause == HG_AIR_CAUSE_CO2 ? (float)indoor->co2_ppm :
                      indoor->cause == HG_AIR_CAUSE_VOC ? (float)indoor->voc_index :
                      indoor->pm2_5;
        set_line(out, HG_GLANCE_LINE_INDOOR_POOR, value);
    } else if (aqi_bad) {
        set_line(out, HG_GLANCE_LINE_OUTDOOR_AQI, (float)weather->us_aqi);
        out->aqi_category = weather->aqi_category;
    } else if (briefing) {
        set_line(out, HG_GLANCE_LINE_BRIEFING, 0.0f);
        out->high_c = weather->high_c;
        out->low_c = weather->low_c;
        out->precip_prob_pct = weather->day_precip_prob_pct;
        out->rain_start_hour = weather->rain_start_hour;
        out->day_condition = weather->day_condition;
    } else if (have_weather) {
        float gust = weather->day_gust_max_kph > weather->gust_kph ?
                     weather->day_gust_max_kph : weather->gust_kph;
        float hot = weather->feels_like_c > weather->high_c ? weather->feels_like_c :
                                                               weather->high_c;
        float cold = weather->feels_like_c < weather->low_c ? weather->feels_like_c :
                                                               weather->low_c;
        if (gust >= HG_GLANCE_GUST_KPH) {
            set_line(out, HG_GLANCE_LINE_GUSTS, gust);
        } else if (hot >= HG_GLANCE_HEAT_C) {
            set_line(out, HG_GLANCE_LINE_HEAT, hot);
        } else if (cold <= HG_GLANCE_COLD_C) {
            set_line(out, HG_GLANCE_LINE_COLD, cold);
        } else if (weather->is_day && weather->uv_max >= HG_GLANCE_UV_INDEX) {
            set_line(out, HG_GLANCE_LINE_UV, weather->uv_max);
        }
    }
}
