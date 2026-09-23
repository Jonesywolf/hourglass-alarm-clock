#include "hg/ui/weather_screen.h"

#include "hg/ui/fonts.h"
#include "hg/ui/theme.h"

static lv_obj_t *title_label;
static lv_obj_t *body_label;

void hg_weather_screen_create(lv_obj_t *parent) {
    if (!parent) {
        return;
    }

    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, hg_theme_bg(), 0);

    title_label = lv_label_create(parent);
    lv_obj_set_style_text_font(title_label, HG_FONT_ALARM, 0);
    lv_obj_set_style_text_color(title_label, hg_theme_fg(), 0);
    lv_label_set_text(title_label, "Weather");
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, -20);

    body_label = lv_label_create(parent);
    lv_obj_set_style_text_font(body_label, HG_FONT_STATUS, 0);
    lv_obj_set_style_text_color(body_label, hg_theme_muted(), 0);
    lv_label_set_text(body_label, "Open-Meteo coming soon");
    lv_obj_align(body_label, LV_ALIGN_CENTER, 0, 16);
}

void hg_weather_screen_update(const hg_weather_snapshot_t *weather,
                              const hg_indoor_snapshot_t *indoor) {
    (void)weather;
    (void)indoor;

    if (!title_label || !body_label) {
        return;
    }

    lv_obj_set_style_text_color(title_label, hg_theme_fg(), 0);
    lv_obj_set_style_text_color(body_label, hg_theme_muted(), 0);
}
