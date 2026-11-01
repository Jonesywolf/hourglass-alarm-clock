#include "hg/ui/weather_icons.h"

#include "hg/ui/icons.h"
#include "hg/ui/theme.h"
#include "hg/log.h"

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

void hg_wx_icon_align_temperature(lv_obj_t *img, lv_obj_t *temperature, lv_coord_t center_y) {
    struct bounds_t {
        bool ready;
        lv_coord_t top;
        lv_coord_t bottom;
    };
    static bounds_t art_bounds[HG_WX_COUNT][2][2] = {};
    const void *source = lv_img_get_src(img);
    bounds_t *bounds = nullptr;
    for (unsigned condition = 0; condition < HG_WX_COUNT && !bounds; ++condition) {
        for (unsigned night = 0; night < 2 && !bounds; ++night) {
            for (unsigned size = 0; size < 2; ++size) {
                const icon_pair_t &pair = night ? icons[condition].night[size] :
                                                  icons[condition].day[size];
                if (source != pair.color && source != pair.mono) {
                    continue;
                }
                bounds = &art_bounds[condition][night][size];
                if (!bounds->ready) {
                    bounds->top = pair.mono->header.h;
                    bounds->bottom = -1;
                    for (unsigned y = 0; y < pair.mono->header.h; ++y) {
                        for (unsigned x = 0; x < pair.mono->header.w; ++x) {
                            // Exclude nearly transparent antialiasing fringes.
                            if (pair.mono->data[y * pair.mono->header.w + x] > 32) {
                                bounds->top = LV_MIN(bounds->top, static_cast<lv_coord_t>(y));
                                bounds->bottom = y;
                                break;
                            }
                        }
                    }
                    bounds->ready = true;
                }
                break;
            }
        }
    }
    if (!bounds || bounds->bottom < bounds->top) {
        HG_ERROR("weather-ui", "Cannot align temperature: missing weather artwork bounds\n");
        return;
    }
    const lv_font_t *font = lv_obj_get_style_text_font(temperature, LV_PART_MAIN);
    const char *text = lv_label_get_text(temperature);
    lv_coord_t top = font->line_height;
    lv_coord_t bottom = -1;
    uint32_t offset = 0;
    while (text[offset]) {
        uint32_t character = _lv_txt_encoded_next(text, &offset);
        lv_font_glyph_dsc_t glyph;
        if (!lv_font_get_glyph_dsc(font, &glyph, character, 0)) {
            HG_ERROR("weather-ui", "Cannot align temperature: missing glyph %lu\n",
                     static_cast<unsigned long>(character));
            return;
        }
        if (glyph.box_h) {
            lv_coord_t glyph_top = font->line_height - font->base_line - glyph.box_h - glyph.ofs_y;
            top = LV_MIN(top, glyph_top);
            bottom = LV_MAX(bottom, glyph_top + glyph.box_h - 1);
        }
    }
    if (bottom < top) {
        HG_ERROR("weather-ui", "Cannot align an empty temperature\n");
        return;
    }
    lv_obj_set_y(img, center_y - (bounds->top + bounds->bottom + 1) / 2);
    lv_obj_set_y(temperature, center_y - (top + bottom + 1) / 2);
}
