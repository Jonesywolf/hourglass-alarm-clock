#include "hg/ui/overlays.h"

#include <lvgl.h>

#include "hg/config.h"
#include "hg/ui/fonts.h"
#include "hg/ui/theme.h"

typedef enum { OVERLAY_NONE, OVERLAY_RINGING, OVERLAY_CONFIRM } overlay_mode_t;

static overlay_mode_t mode = OVERLAY_NONE;
static lv_obj_t *root;
static lv_obj_t *icon_label;
static lv_obj_t *title_label;
static lv_obj_t *time_label;
static lv_obj_t *touch_text;
static lv_obj_t *touch_arrow;
static lv_obj_t *knob_text;
static lv_obj_t *knob_arrow;
static int32_t alarm_icon_opacity = LV_OPA_COVER;
static uint32_t alarm_refresh_period = 0;
static uint32_t alarm_animation_period = 0;
static bool prev_snoozed = false;
static int shown_key = -1;
static uint32_t confirm_until_ms = 0;
static lv_obj_t *volume_root;
static lv_obj_t *volume_bar;
static lv_obj_t *volume_icon;
static bool volume_shown = false;
static bool volume_colors_valid = false;
static bool volume_icon_high = true;
static lv_color_t volume_fg, volume_bg;
static const int volume_width = 28;
static const lv_point_t touch_arrow_points[] = {{14, 36}, {14, 2}, {4, 13}, {14, 2}, {24, 13}};
static const lv_point_t knob_arrow_points[] = {{2, 13}, {36, 13}, {25, 3}, {36, 13}, {25, 23}};

static void pulse_alarm_icon(void *object, int32_t opacity) {
    const int32_t step = 8;
    int32_t level = LV_OPA_TRANSP + ((opacity - LV_OPA_TRANSP + step / 2) / step) * step;
    if (level > LV_OPA_COVER) {
        level = LV_OPA_COVER;
    } else if (level < LV_OPA_60) {
        level = LV_OPA_60;
    }
    if (level != alarm_icon_opacity) {
        alarm_icon_opacity = level;
        lv_obj_set_style_text_opa(static_cast<lv_obj_t *>(object), level, 0);
    }
}

static void start_alarm_pulse(void) {
    lv_timer_t *refresh = _lv_disp_get_refr_timer(lv_obj_get_disp(icon_label));
    lv_timer_t *animations = lv_anim_get_timer();
    alarm_refresh_period = refresh->period;
    alarm_animation_period = animations->period;
    // Mixed partial/full paints need headroom before the next display transfer.
    lv_timer_set_period(refresh, 16);
    lv_timer_set_period(animations, 16);
    alarm_icon_opacity = LV_OPA_COVER;
    lv_obj_set_style_text_opa(icon_label, LV_OPA_COVER, 0);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, icon_label);
    lv_anim_set_exec_cb(&animation, pulse_alarm_icon);
    lv_anim_set_values(&animation, LV_OPA_COVER, LV_OPA_60);
    lv_anim_set_time(&animation, 1000);
    lv_anim_set_playback_time(&animation, 1000);
    lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_start(&animation);
}

static void stop_alarm_pulse(void) {
    if (icon_label) {
        lv_anim_del(icon_label, pulse_alarm_icon);
        lv_obj_set_style_text_opa(icon_label, LV_OPA_COVER, 0);
        if (alarm_refresh_period) {
            lv_timer_set_period(_lv_disp_get_refr_timer(lv_obj_get_disp(icon_label)),
                                alarm_refresh_period);
            lv_timer_set_period(lv_anim_get_timer(), alarm_animation_period);
            alarm_refresh_period = alarm_animation_period = 0;
        }
    }
    alarm_icon_opacity = LV_OPA_COVER;
}

static void volume_x(void *object, int32_t x) {
    lv_obj_t *panel = static_cast<lv_obj_t *>(object);
    lv_obj_set_x(panel, x);
    // Render each animation step without waiting on a separately phased refresh timer.
    lv_timer_ready(_lv_disp_get_refr_timer(lv_obj_get_disp(panel)));
}

static void volume_slide_finished(lv_anim_t *animation) {
    if (animation->end_value == HG_DISP_HOR_RES) {
        lv_obj_add_flag(volume_root, LV_OBJ_FLAG_HIDDEN);
    }
}

static void volume_slide(bool show) {
    lv_anim_del(volume_root, volume_x);
    lv_obj_clear_flag(volume_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(volume_root);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, volume_root);
    lv_anim_set_exec_cb(&animation, volume_x);
    lv_anim_set_values(&animation, lv_obj_get_x(volume_root),
                        HG_DISP_HOR_RES - (show ? volume_width : 0));
    lv_anim_set_time(&animation, HG_VOLUME_SLIDE_MS);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&animation, volume_slide_finished);
    lv_anim_start(&animation);
}

static void create_volume(void) {
    volume_root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(volume_root);
    lv_obj_set_size(volume_root, volume_width, 160);
    lv_obj_set_pos(volume_root, HG_DISP_HOR_RES, (HG_DISP_VER_RES - 160) / 2);
    lv_obj_clear_flag(volume_root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(volume_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(volume_root, LV_OPA_COVER, 0);
    volume_bar = lv_bar_create(volume_root);
    lv_obj_remove_style_all(volume_bar);
    lv_obj_set_size(volume_bar, 8, 128);
    lv_obj_align(volume_bar, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_clear_flag(volume_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_bar_set_range(volume_bar, 0, 100);
    lv_obj_set_style_bg_opa(volume_bar, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(volume_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(volume_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(volume_bar, 4, LV_PART_INDICATOR);
    volume_icon = lv_label_create(volume_root);
    lv_obj_set_style_text_font(volume_icon, HG_FONT_VOLUME_ICON, 0);
    lv_label_set_text(volume_icon, HG_ICON_VOLUME_HIGH);
    volume_icon_high = true;
    lv_obj_align(volume_icon, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_clear_flag(volume_icon, LV_OBJ_FLAG_CLICKABLE);
}

void hg_overlays_set_volume(const hg_volume_snapshot_t *volume) {
    bool show = volume->visible && mode == OVERLAY_NONE;
    if (!volume_root && !show) {
        return;
    }
    if (!volume_root) {
        create_volume();
    }
    if (show) {
        lv_color_t fg = hg_theme_fg(), bg = hg_theme_bg();
        if (!volume_colors_valid || lv_color_to32(fg) != lv_color_to32(volume_fg) ||
            lv_color_to32(bg) != lv_color_to32(volume_bg)) {
            volume_colors_valid = true;
            volume_fg = fg;
            volume_bg = bg;
            lv_obj_set_style_bg_color(volume_root, bg, 0);
            lv_obj_set_style_bg_color(volume_bar, fg, LV_PART_MAIN);
            lv_obj_set_style_bg_color(volume_bar, fg, LV_PART_INDICATOR);
            lv_obj_set_style_text_color(volume_icon, fg, 0);
        }
        lv_bar_set_value(volume_bar, volume->volume_pct, LV_ANIM_OFF);
        bool high = volume->volume_pct > 50;
        if (high != volume_icon_high) {
            volume_icon_high = high;
            lv_label_set_text(volume_icon, high ? HG_ICON_VOLUME_HIGH : HG_ICON_VOLUME_LOW);
        }
    }
    if (!show && (volume->dismiss_immediately || mode != OVERLAY_NONE)) {
        lv_anim_del(volume_root, volume_x);
        lv_obj_add_flag(volume_root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(volume_root, HG_DISP_HOR_RES);
    } else if (show != volume_shown) {
        volume_slide(show);
    }
    volume_shown = show;
}

bool hg_overlays_volume_animating(void) {
    return volume_root && lv_anim_get(volume_root, volume_x) != NULL;
}

#ifdef HG_RUNTIME_TEST
bool hg_overlays_test_volume(int *x, int *value) {
    if (volume_root) {
        lv_obj_update_layout(volume_root);
    }
    *x = volume_root ? lv_obj_get_x(volume_root) : HG_DISP_HOR_RES;
    *value = volume_bar ? lv_bar_get_value(volume_bar) : 0;
    return volume_shown;
}

lv_obj_t *hg_overlays_test_volume_root(void) {
    return volume_root;
}

lv_obj_t *hg_overlays_test_alarm_root(void) {
    return root;
}
#endif

static lv_obj_t *make_label(const lv_font_t *font) {
    lv_obj_t *label = lv_label_create(root);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static lv_obj_t *make_arrow(const lv_point_t *points, uint16_t point_count,
                            lv_align_t align, lv_coord_t x, lv_coord_t y) {
    lv_obj_t *arrow = lv_line_create(root);
    lv_obj_remove_style_all(arrow);
    lv_line_set_points(arrow, points, point_count);
    lv_obj_set_style_line_width(arrow, 3, 0);
    lv_obj_set_style_line_rounded(arrow, true, 0);
    lv_obj_clear_flag(arrow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(arrow, align, x, y);
    return arrow;
}

static void destroy(void) {
    stop_alarm_pulse();
    if (root) {
        lv_obj_del(root);
    }
    root = icon_label = title_label = time_label = NULL;
    touch_text = touch_arrow = NULL;
    knob_text = knob_arrow = NULL;
    mode = OVERLAY_NONE;
}

static lv_color_t last_fg, last_bg, last_alarm;
static bool have_last_colors = false;

static void apply_colors(void) {
    if (!root) {
        return;
    }
    lv_color_t fg = hg_theme_fg(), bg = hg_theme_bg(), alarm = hg_theme_alarm();
    if (have_last_colors && lv_color_to32(fg) == lv_color_to32(last_fg) &&
        lv_color_to32(bg) == lv_color_to32(last_bg) &&
        lv_color_to32(alarm) == lv_color_to32(last_alarm)) {
        return;
    }
    last_fg = fg;
    last_bg = bg;
    last_alarm = alarm;
    have_last_colors = true;
    lv_obj_set_style_bg_color(root, bg, 0);
    lv_obj_set_style_text_color(icon_label, alarm, 0);
    lv_obj_set_style_text_color(time_label, alarm, 0);
    lv_obj_set_style_text_color(title_label, fg, 0);
    lv_obj_set_style_line_color(touch_arrow, hg_theme_muted(), 0);
    lv_obj_set_style_text_color(touch_text, fg, 0);
    lv_obj_set_style_line_color(knob_arrow, hg_theme_muted(), 0);
    lv_obj_set_style_text_color(knob_text, fg, 0);
}

static void create(void) {
    root = lv_obj_create(lv_layer_top());
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_radius(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(root, LV_ALIGN_CENTER, 0, 0);

    icon_label = make_label(HG_FONT_ALARM_ICON);
    lv_label_set_text(icon_label, HG_ICON_ALARM);
    lv_obj_align(icon_label, LV_ALIGN_TOP_MID, 0, 14);

    title_label = make_label(HG_FONT_STATUS);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 84);

    time_label = make_label(HG_FONT_ALARM);
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 104);

    touch_text = make_label(HG_FONT_STATUS);
    lv_label_set_text(touch_text, "Tap to snooze");
    lv_obj_align(touch_text, LV_ALIGN_TOP_MID, -24, 50);
    touch_arrow = make_arrow(touch_arrow_points,
                             sizeof(touch_arrow_points) / sizeof(touch_arrow_points[0]),
                             LV_ALIGN_TOP_MID, -24, 8);

    knob_text = make_label(HG_FONT_STATUS);
    lv_label_set_text(knob_text, "Press to stop");
    lv_obj_align(knob_text, LV_ALIGN_RIGHT_MID, -8, 34);
    knob_arrow = make_arrow(knob_arrow_points,
                            sizeof(knob_arrow_points) / sizeof(knob_arrow_points[0]),
                            LV_ALIGN_RIGHT_MID, -8, -3);
    have_last_colors = false;
    apply_colors();
}

static void set_time_text(const hg_alarm_snapshot_t *alarm) {
    int hour12 = alarm->hour % 12;
    if (hour12 == 0) {
        hour12 = 12;
    }
    lv_label_set_text_fmt(time_label, "%d:%02d %s", hour12, alarm->minute,
                          alarm->hour >= 12 ? "pm" : "am");
    lv_obj_align(time_label, LV_ALIGN_TOP_MID,
                 mode == OVERLAY_RINGING ? -24 : 0,
                 mode == OVERLAY_RINGING ? 163 : 104);
}

static void show_ringing(const hg_alarm_snapshot_t *alarm) {
    int key = alarm->hour * 60 + alarm->minute + (alarm->snoozed ? 100000 : 0);
    if (mode == OVERLAY_RINGING && key == shown_key) {
        return;
    }
    bool was_ringing = mode == OVERLAY_RINGING;
    shown_key = key;
    if (!root) {
        create();
    }
    mode = OVERLAY_RINGING;
    lv_obj_add_flag(title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(icon_label, LV_ALIGN_TOP_MID, -24, 80);
    set_time_text(alarm);
    lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(touch_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(touch_arrow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(knob_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(knob_arrow, LV_OBJ_FLAG_HIDDEN);
    if (!was_ringing) {
        start_alarm_pulse();
    }
    lv_obj_move_foreground(root);
}

static void show_confirm(const hg_alarm_snapshot_t *alarm) {
    stop_alarm_pulse();
    if (!root) {
        create();
    }
    mode = OVERLAY_CONFIRM;
    lv_obj_align(icon_label, LV_ALIGN_TOP_MID, 0, 14);
    confirm_until_ms = lv_tick_get() + HG_ALARM_CONFIRM_MS;
    lv_label_set_text(title_label, alarm->snoozed ? "Snoozed until" :
                                   alarm->enabled ? "Next alarm" : "Alarm off");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 84);
    lv_obj_clear_flag(title_label, LV_OBJ_FLAG_HIDDEN);
    if (alarm->enabled) {
        set_time_text(alarm);
        lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(touch_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(touch_arrow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(knob_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(knob_arrow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(root);
}

void hg_overlays_init(void) {
    volume_root = volume_bar = volume_icon = NULL;
    volume_shown = false;
    volume_colors_valid = false;
    root = icon_label = title_label = time_label = NULL;
    touch_text = touch_arrow = NULL;
    knob_text = knob_arrow = NULL;
    mode = OVERLAY_NONE;
    prev_snoozed = false;
}

void hg_overlays_set_alarm(const hg_alarm_snapshot_t *alarm) {
    if (!alarm) {
        return;
    }
    if (alarm->ringing) {
        show_ringing(alarm);
    } else if (mode == OVERLAY_RINGING || (mode == OVERLAY_NONE && prev_snoozed && !alarm->snoozed)) {
        show_confirm(alarm);
    }
    prev_snoozed = alarm->snoozed;
}

void hg_overlays_update(void) {
    if (mode == OVERLAY_CONFIRM && (int32_t)(lv_tick_get() - confirm_until_ms) >= 0) {
        destroy();
    }
    apply_colors();
}

bool hg_overlays_alarm_visible(void) {
    return mode != OVERLAY_NONE;
}
