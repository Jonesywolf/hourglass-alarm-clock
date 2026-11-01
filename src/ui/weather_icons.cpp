#include "hg/ui/weather_icons.h"

#include "hg/ui/icons.h"
#include "hg/ui/theme.h"

typedef struct {
    const lv_img_dsc_t *color;
    const lv_img_dsc_t *mono;
} icon_pair_t;

#define HG_ICON_PAIR(name, size) {&hg_icon_##name##_##size, &hg_icon_##name##_##size##_mono}
#define HG_ICON_SIZES(name) {HG_ICON_PAIR(name, 40), HG_ICON_PAIR(name, 80)}

typedef struct {
    icon_pair_t day[2];
    icon_pair_t night[2];
} condition_icons_t;

#define HG_SAME(name) {HG_ICON_SIZES(name), HG_ICON_SIZES(name)}
#define HG_DAY_NIGHT(day, night) {HG_ICON_SIZES(day), HG_ICON_SIZES(night)}

/* Indexed by hg_weather_condition_t. */
static const condition_icons_t icons[HG_WX_COUNT] = {
    HG_SAME(not_available),
    HG_DAY_NIGHT(clear_day, clear_night),
    HG_DAY_NIGHT(partly_cloudy_day, partly_cloudy_night),
    HG_SAME(overcast),
    HG_DAY_NIGHT(fog_day, fog_night),
    HG_SAME(drizzle),
    HG_SAME(rain),
    HG_SAME(rain),
    HG_SAME(sleet),
    HG_SAME(snow),
    HG_SAME(snow),
    HG_DAY_NIGHT(thunder_day, thunder_night),
    HG_SAME(hail),
};

/* Re-setting an identical source would still invalidate the image every UI tick. */
static void set_src(lv_obj_t *img, const lv_img_dsc_t *src) {
    if (lv_img_get_src(img) != src) {
        lv_img_set_src(img, src);
    }
}

void hg_wx_icon_apply(lv_obj_t *img, hg_weather_condition_t condition, bool is_day,
                      hg_wx_icon_size_t size, bool muted) {
    if (!img) {
        return;
    }
    unsigned index = (unsigned)condition < HG_WX_COUNT ? (unsigned)condition : 0U;
    unsigned slot = size == HG_WX_ICON_LARGE ? 1U : 0U;
    const icon_pair_t *pair = is_day ? &icons[index].day[slot] : &icons[index].night[slot];
    if (hg_theme_nightlight()) {
        set_src(img, pair->mono);
        lv_obj_set_style_img_recolor(img, muted ? hg_theme_muted() : hg_theme_fg(), 0);
        lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
        lv_obj_set_style_img_opa(img, LV_OPA_COVER, 0);
    } else {
        set_src(img, pair->color);
        lv_obj_set_style_img_recolor_opa(img, LV_OPA_TRANSP, 0);
        lv_obj_set_style_img_opa(img, muted ? LV_OPA_50 : LV_OPA_COVER, 0);
    }
}
