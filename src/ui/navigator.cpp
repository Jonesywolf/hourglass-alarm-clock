#include "hg/ui/navigator.h"

#include <lvgl.h>

#include "hg/config.h"
#include "hg/log.h"
#include "hg/ui/theme.h"

static hg_page_id_t current_page = HG_PAGE_CLOCK;
static hg_page_id_t target_page = HG_PAGE_CLOCK;
static lv_obj_t *strip = nullptr;
static lv_obj_t *clock_page = nullptr;
static lv_obj_t *weather_page = nullptr;
static bool sliding = false;
static uint32_t weather_entered_ms = 0;

static lv_coord_t hg_navigator_strip_x_for(hg_page_id_t page) {
    return (page == HG_PAGE_WEATHER) ? (lv_coord_t)(-HG_DISP_HOR_RES) : 0;
}

static void hg_navigator_anim_x(void *obj, int32_t value) {
    lv_obj_set_x((lv_obj_t *)obj, (lv_coord_t)value);
}

static void hg_navigator_slide_ready(lv_anim_t *anim) {
    lv_coord_t end_x = hg_navigator_strip_x_for(target_page);
    if (strip) {
        lv_obj_set_x(strip, end_x);
    }
    sliding = false;
    current_page = target_page;
    if (current_page == HG_PAGE_WEATHER) {
        weather_entered_ms = lv_tick_get();
    }
    HG_LOG("nav", "slide ready page=%d x=%d\n", (int)current_page, (int)end_x);
    (void)anim;
}

static void hg_navigator_stop_slide(void) {
    if (strip) {
        lv_anim_del(strip, hg_navigator_anim_x);
    }
}

static void hg_navigator_start_slide(lv_coord_t to_x) {
    if (!strip) {
        return;
    }

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, strip);
    lv_anim_set_values(&anim, lv_obj_get_x(strip), to_x);
    lv_anim_set_time(&anim, HG_PAGE_SLIDE_MS);
    lv_anim_set_exec_cb(&anim, hg_navigator_anim_x);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&anim, hg_navigator_slide_ready);
    lv_anim_start(&anim);
}

static lv_obj_t *hg_navigator_make_page(lv_obj_t *parent, lv_coord_t x) {
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_set_size(page, HG_DISP_HOR_RES, HG_DISP_VER_RES);
    lv_obj_set_pos(page, x, 0);
    lv_obj_set_style_bg_color(page, hg_theme_bg(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_set_style_shadow_width(page, 0, 0);
    lv_obj_set_style_outline_width(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
    return page;
}

void hg_navigator_init(void) {
    lv_obj_t *root = lv_scr_act();
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root, hg_theme_bg(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);

    strip = lv_obj_create(root);
    lv_obj_set_size(strip, HG_DISP_HOR_RES * 2, HG_DISP_VER_RES);
    lv_obj_set_pos(strip, 0, 0);
    lv_obj_set_style_bg_opa(strip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(strip, 0, 0);
    lv_obj_set_style_pad_all(strip, 0, 0);
    lv_obj_set_style_radius(strip, 0, 0);
    lv_obj_set_style_shadow_width(strip, 0, 0);
    lv_obj_set_style_outline_width(strip, 0, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(strip, LV_SCROLLBAR_MODE_OFF);

    clock_page = hg_navigator_make_page(strip, 0);
    weather_page = hg_navigator_make_page(strip, HG_DISP_HOR_RES);

    current_page = HG_PAGE_CLOCK;
    target_page = HG_PAGE_CLOCK;
    sliding = false;
    weather_entered_ms = 0;
}

void hg_navigator_update(void) {
    if (target_page != HG_PAGE_WEATHER || sliding) {
        return;
    }

    if ((lv_tick_get() - weather_entered_ms) >= HG_WEATHER_IDLE_RETURN_MS) {
        hg_navigator_show_page(HG_PAGE_CLOCK);
    }
}

void hg_navigator_show_page(hg_page_id_t page) {
    if (page != HG_PAGE_CLOCK && page != HG_PAGE_WEATHER) {
        return;
    }

    if (!strip || !clock_page || !weather_page) {
        return;
    }

    if (page == target_page && !sliding) {
        return;
    }

    target_page = page;
    sliding = true;
    if (page == HG_PAGE_WEATHER) {
        weather_entered_ms = lv_tick_get();
    }

    hg_navigator_stop_slide();
    HG_LOG("nav", "slide to page=%d from_x=%d\n", (int)page, (int)lv_obj_get_x(strip));
    hg_navigator_start_slide(hg_navigator_strip_x_for(page));
}

void hg_navigator_next(void) {
    hg_navigator_show_page(target_page == HG_PAGE_CLOCK ? HG_PAGE_WEATHER : HG_PAGE_CLOCK);
}

void hg_navigator_prev(void) {
    hg_navigator_show_page(target_page == HG_PAGE_CLOCK ? HG_PAGE_WEATHER : HG_PAGE_CLOCK);
}

hg_page_id_t hg_navigator_current(void) {
    return target_page;
}

bool hg_navigator_is_sliding(void) {
    return sliding;
}

lv_obj_t *hg_navigator_page(hg_page_id_t page) {
    if (page == HG_PAGE_CLOCK) {
        return clock_page;
    }
    if (page == HG_PAGE_WEATHER) {
        return weather_page;
    }
    return nullptr;
}
