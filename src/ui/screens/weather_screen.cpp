#include "hg/ui/weather_screen.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hg/config.h"
#include "hg/ui/fonts.h"
#include "hg/ui/theme.h"
#include "hg/ui/ui_icon.h"
#include "hg/ui/weather_format.h"
#include "hg/ui/weather_icons.h"

#define COLUMNS HG_WEATHER_HOURLY_VISIBLE
#define DOT " \xC2\xB7 "

typedef struct {
    lv_obj_t *hour;
    lv_obj_t *icon;
    lv_obj_t *temp;
    lv_obj_t *precip;
} column_t;

typedef struct {
    lv_obj_t *box;
    lv_obj_t *icon;
    lv_obj_t *text;
    uint32_t color;
    uint32_t icon_color;
} chip_t;

static lv_obj_t *hero_icon;
static lv_obj_t *temp_label;
static lv_obj_t *status_label;
static lv_obj_t *detail_label;
static lv_obj_t *rain_icon;
static lv_obj_t *rain_label;
static lv_obj_t *uv_icon;
static lv_obj_t *uv_label;
static column_t columns[COLUMNS];
static lv_obj_t *track;
static lv_obj_t *thumb;
static chip_t wind_chip;
static chip_t aqi_chip;
static chip_t indoor_chip;

/* Last rendered inputs: LVGL text/style setters re-layout even when unchanged, so the
 * 1 Hz UI tick must not repaint a page whose data did not move. */
static bool rendered;
static bool shown_nightlight;
static hg_unit_system_t shown_units;
static hg_weather_snapshot_t shown_weather;
static hg_weather_view_t shown_view;

static const lv_coord_t strip_x = 10;
static const lv_coord_t column_w = 60;
static const lv_coord_t track_w = COLUMNS * 60;

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_label_set_text(label, "");
    return label;
}

static void set_visible(lv_obj_t *obj, bool visible) {
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void make_chip(lv_obj_t *row, chip_t *chip, hg_ui_icon_id_t icon) {
    chip->box = lv_obj_create(row);
    lv_obj_remove_style_all(chip->box);
    lv_obj_set_size(chip->box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(chip->box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chip->box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(chip->box, 4, 0);
    chip->icon = hg_ui_icon_create(chip->box, icon, 18);
    chip->text = make_label(chip->box, HG_FONT_STATUS);
    chip->color = 0;
    chip->icon_color = 0;
}

static void set_chip(chip_t *chip, const char *text, lv_color_t color) {
    if (strcmp(lv_label_get_text(chip->text), text) != 0) {
        lv_label_set_text(chip->text, text);
    }
    uint32_t value = lv_color_to32(color);
    if (value != chip->color) {
        chip->color = value;
        lv_obj_set_style_text_color(chip->text, color, 0);
    }
    lv_color_t muted = hg_theme_muted();
    value = lv_color_to32(muted);
    if (value != chip->icon_color) {
        chip->icon_color = value;
        hg_ui_icon_set_color(chip->icon, muted);
    }
}

void hg_weather_screen_create(lv_obj_t *parent) {
    if (!parent) {
        return;
    }
    rendered = false;
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, hg_theme_bg(), 0);

    hero_icon = lv_img_create(parent);
    lv_obj_set_pos(hero_icon, 6, 12);

    temp_label = make_label(parent, HG_FONT_WEATHER_TEMP);
    lv_obj_set_pos(temp_label, 94, 8);
    detail_label = make_label(parent, HG_FONT_STATUS);
    lv_label_set_long_mode(detail_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(detail_label, 220);
    lv_obj_set_pos(detail_label, 96, 78);
    status_label = make_label(parent, HG_FONT_STATUS);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(status_label, 220);
    lv_obj_set_pos(status_label, 96, 96);

    rain_icon = hg_ui_icon_create(parent, HG_UI_ICON_DROP, 18);
    lv_obj_set_pos(rain_icon, 236, 30);
    rain_label = make_label(parent, HG_FONT_STATUS);
    lv_obj_set_pos(rain_label, 258, 31);
    uv_icon = hg_ui_icon_create(parent, HG_UI_ICON_SUN, 18);
    lv_obj_set_pos(uv_icon, 236, 52);
    uv_label = make_label(parent, HG_FONT_STATUS);
    lv_obj_set_pos(uv_label, 258, 53);

    for (unsigned i = 0; i < COLUMNS; ++i) {
        lv_coord_t x = strip_x + (lv_coord_t)i * column_w;
        column_t *column = &columns[i];
        column->hour = make_label(parent, HG_FONT_STATUS);
        column->icon = lv_img_create(parent);
        column->temp = make_label(parent, HG_FONT_STATUS);
        column->precip = make_label(parent, HG_FONT_STATUS);
        lv_obj_t *labels[] = {column->hour, column->temp, column->precip};
        lv_coord_t rows[] = {118, 176, 192};
        for (unsigned j = 0; j < 3; ++j) {
            lv_obj_set_width(labels[j], column_w);
            lv_obj_set_style_text_align(labels[j], LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(labels[j], x, rows[j]);
        }
        lv_obj_set_pos(column->icon, x + (column_w - 40) / 2, 136);
    }

    track = lv_obj_create(parent);
    lv_obj_remove_style_all(track);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(track, 1, 0);
    lv_obj_set_size(track, track_w, 2);
    lv_obj_set_pos(track, strip_x, 212);
    thumb = lv_obj_create(track);
    lv_obj_remove_style_all(thumb);
    lv_obj_set_style_bg_opa(thumb, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(thumb, 1, 0);
    lv_obj_set_height(thumb, 2);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 308, LV_SIZE_CONTENT);
    lv_obj_set_pos(row, 6, 218);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    make_chip(row, &wind_chip, HG_UI_ICON_WIND);
    make_chip(row, &aqi_chip, HG_UI_ICON_DOTS_NINE);
    make_chip(row, &indoor_chip, HG_UI_ICON_HOUSE);
}

static void render_hero(const hg_weather_snapshot_t *weather, hg_unit_system_t units) {
    bool muted = weather->status == HG_SAMPLE_STALE;
    lv_color_t fg = muted ? hg_theme_muted() : hg_theme_fg();
    char text[64], a[16], b[16], c[16];
    lv_obj_set_style_text_color(temp_label, fg, 0);
    lv_obj_set_style_text_color(status_label, hg_theme_muted(), 0);
    lv_obj_set_style_text_color(detail_label, hg_theme_muted(), 0);
    if (!weather->valid) {
        hg_wx_icon_apply(hero_icon, HG_WX_UNKNOWN, true, HG_WX_ICON_LARGE, true);
        lv_label_set_text(temp_label, "--\xC2\xB0");
        hg_wx_icon_align_temperature(hero_icon, temp_label, 44);
        lv_label_set_text(status_label, weather->status == HG_SAMPLE_ERROR ?
                          "Weather unavailable" : "Fetching weather\xE2\x80\xA6");
        set_visible(status_label, true);
        lv_label_set_text(detail_label, weather->location[0] ? "Will retry shortly" :
                          "Waiting for Wi-Fi and location");
        set_visible(rain_icon, false);
        set_visible(rain_label, false);
        set_visible(uv_icon, false);
        set_visible(uv_label, false);
        return;
    }
    hg_wx_icon_apply(hero_icon, weather->condition, weather->is_day, HG_WX_ICON_LARGE, muted);
    hg_wx_format_temp(text, sizeof(text), weather->temp_c, units);
    lv_label_set_text(temp_label, text);
    hg_wx_icon_align_temperature(hero_icon, temp_label, 44);
    lv_label_set_text(status_label, muted ? "Out of date" : "");
    set_visible(status_label, muted);
    hg_wx_format_temp(a, sizeof(a), weather->feels_like_c, units);
    hg_wx_format_temp(b, sizeof(b), weather->high_c, units);
    hg_wx_format_temp(c, sizeof(c), weather->low_c, units);
    snprintf(text, sizeof(text), "Feels %s" DOT "H %s  L %s", a, b, c);
    lv_label_set_text(detail_label, text);

    hg_ui_icon_set_color(rain_icon, hg_wx_rain_color());
    lv_obj_set_style_text_color(rain_label, fg, 0);
    lv_label_set_text_fmt(rain_label, "%u%%", (unsigned)weather->day_precip_prob_pct);
    hg_ui_icon_set_color(uv_icon, hg_theme_signal(lv_color_hex(0xFBBF24)));
    lv_obj_set_style_text_color(uv_label, fg, 0);
    lv_label_set_text_fmt(uv_label, "UV %ld", isnan(weather->uv_max) ? 0L :
                          lroundf(weather->uv_max));
    set_visible(rain_icon, true);
    set_visible(rain_label, true);
    set_visible(uv_icon, true);
    set_visible(uv_label, true);
}

static void render_hourly(const hg_weather_snapshot_t *weather, const hg_weather_view_t *view,
                          hg_unit_system_t units) {
    unsigned count = weather->valid ? weather->hourly_count : 0U;
    if (count > HG_WEATHER_HOURS) {
        count = HG_WEATHER_HOURS;
    }
    unsigned max_offset = count > COLUMNS ? count - COLUMNS : 0U;
    unsigned offset = view->hourly_offset > max_offset ? max_offset : view->hourly_offset;
    bool muted = weather->status == HG_SAMPLE_STALE;
    for (unsigned i = 0; i < COLUMNS; ++i) {
        column_t *column = &columns[i];
        unsigned index = offset + i;
        bool shown = index < count;
        set_visible(column->hour, shown);
        set_visible(column->icon, shown);
        set_visible(column->temp, shown);
        if (!shown) {
            set_visible(column->precip, false);
            continue;
        }
        const hg_weather_hour_t *hour = &weather->hourly[index];
        char text[16];
        if (index == 0) {
            snprintf(text, sizeof(text), "Now");
        } else {
            hg_wx_format_hour(text, sizeof(text), hour->hour_24);
        }
        lv_label_set_text(column->hour, text);
        lv_obj_set_style_text_color(column->hour, hg_theme_muted(), 0);
        hg_wx_icon_apply(column->icon, (hg_weather_condition_t)hour->condition, hour->is_day,
                         HG_WX_ICON_SMALL, muted);
        hg_wx_format_temp(text, sizeof(text), hour->temp_c, units);
        lv_label_set_text(column->temp, text);
        lv_obj_set_style_text_color(column->temp, muted ? hg_theme_muted() : hg_theme_fg(), 0);
        /* Precipitation chance only where it matters, so the strip stays quiet on dry days. */
        bool wet = hour->precip_prob_pct >= 20;
        if (wet) {
            lv_label_set_text_fmt(column->precip, "%u%%", (unsigned)hour->precip_prob_pct);
            lv_obj_set_style_text_color(column->precip, hg_wx_rain_color(), 0);
        }
        set_visible(column->precip, wet);
    }
    bool scrollable = max_offset > 0;
    set_visible(track, scrollable);
    if (scrollable) {
        lv_coord_t thumb_w = (lv_coord_t)(track_w * COLUMNS / count);
        lv_obj_set_width(thumb, thumb_w);
        lv_obj_set_x(thumb, (lv_coord_t)((track_w - thumb_w) * offset / max_offset));
        lv_obj_set_style_bg_color(track, hg_theme_dim(), 0);
        lv_obj_set_style_bg_color(thumb, hg_theme_muted(), 0);
    }
}

static void render_chips(const hg_weather_page_snapshot_t *snap) {
    const hg_weather_snapshot_t *weather = &snap->weather;
    char text[40], speed[16], gust[16];
    if (weather->valid) {
        hg_wx_format_speed(speed, sizeof(speed), weather->wind_kph, snap->units);
        if (weather->gust_kph >= weather->wind_kph + 15.0f) {
            hg_wx_format_speed(gust, sizeof(gust), weather->gust_kph, snap->units);
            /* Unit only once: "12-35 km/h". */
            char *space = strchr(speed, ' ');
            if (space) {
                *space = '\0';
            }
            snprintf(text, sizeof(text), "%s\xE2\x80\x93%s", speed, gust);
        } else {
            snprintf(text, sizeof(text), "%s", speed);
        }
    } else {
        snprintf(text, sizeof(text), "--");
    }
    set_chip(&wind_chip, text, weather->valid ? hg_theme_fg() : hg_theme_muted());

    if (weather->air_valid) {
        snprintf(text, sizeof(text), "AQI %u", (unsigned)weather->us_aqi);
        set_chip(&aqi_chip, text, weather->air_status == HG_SAMPLE_STALE ? hg_theme_muted() :
                 hg_wx_aqi_color(weather->aqi_category));
    } else {
        set_chip(&aqi_chip, "AQI --", hg_theme_muted());
    }

    const hg_indoor_snapshot_t *indoor = &snap->indoor;
    if (indoor->valid && indoor->level != HG_AIR_UNKNOWN) {
        if (!isnan(indoor->temp_c)) {
            char temp[16];
            hg_wx_format_temp(temp, sizeof(temp), indoor->temp_c, snap->units);
            snprintf(text, sizeof(text), "%s" DOT "%s", hg_wx_air_level_name(indoor->level), temp);
        } else {
            snprintf(text, sizeof(text), "%s", hg_wx_air_level_name(indoor->level));
        }
        set_chip(&indoor_chip, text, indoor->status == HG_SAMPLE_STALE ? hg_theme_muted() :
                 hg_wx_air_level_color(indoor->level));
    } else {
        set_chip(&indoor_chip, "Indoor --", hg_theme_muted());
    }
}

void hg_weather_screen_update(const hg_weather_page_snapshot_t *snap) {
    if (!snap || !hero_icon) {
        return;
    }
    bool nightlight = hg_theme_nightlight();
    bool weather_changed = !rendered || nightlight != shown_nightlight ||
                           snap->units != shown_units ||
                           memcmp(&snap->weather, &shown_weather, sizeof(shown_weather)) != 0;
    bool view_changed = weather_changed ||
                        memcmp(&snap->view, &shown_view, sizeof(shown_view)) != 0;
    if (weather_changed) {
        render_hero(&snap->weather, snap->units);
    }
    if (view_changed) {
        render_hourly(&snap->weather, &snap->view, snap->units);
    }
    /* Indoor samples arrive every second; chips only touch LVGL when their text/color moves. */
    render_chips(snap);
    rendered = true;
    shown_nightlight = nightlight;
    shown_units = snap->units;
    memcpy(&shown_weather, &snap->weather, sizeof(shown_weather));
    memcpy(&shown_view, &snap->view, sizeof(shown_view));
}
