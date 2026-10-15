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
static lv_obj_t *touch_icon;
static lv_obj_t *touch_text;
static lv_obj_t *knob_icon;
static lv_obj_t *knob_text;
static bool prev_snoozed = false;
static int shown_key = -1;
static uint32_t confirm_until_ms = 0;

static lv_obj_t *make_label(const lv_font_t *font) {
    lv_obj_t *label = lv_label_create(root);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static void destroy(void) {
    if (root) {
        lv_obj_del(root);
    }
    root = icon_label = title_label = time_label = NULL;
    touch_icon = touch_text = knob_icon = knob_text = NULL;
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
    lv_obj_set_style_text_color(touch_icon, fg, 0);
    lv_obj_set_style_text_color(touch_text, fg, 0);
    lv_obj_set_style_text_color(knob_icon, fg, 0);
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

    touch_icon = make_label(HG_FONT_ACTION_ICON);
    lv_label_set_text(touch_icon, HG_ICON_SNOOZE);
    lv_obj_align(touch_icon, LV_ALIGN_BOTTOM_MID, -80, -44);
    touch_text = make_label(HG_FONT_STATUS);
    lv_label_set_text_fmt(touch_text, "Tap: snooze %d min", HG_ALARM_SNOOZE_MIN);
    lv_obj_align(touch_text, LV_ALIGN_BOTTOM_MID, -80, -20);

    knob_icon = make_label(HG_FONT_ACTION_ICON);
    lv_label_set_text(knob_icon, HG_ICON_ALARM_OFF);
    lv_obj_align(knob_icon, LV_ALIGN_BOTTOM_MID, 80, -44);
    knob_text = make_label(HG_FONT_STATUS);
    lv_label_set_text(knob_text, "Press knob: stop");
    lv_obj_align(knob_text, LV_ALIGN_BOTTOM_MID, 80, -20);
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
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 104);
}

static void show_ringing(const hg_alarm_snapshot_t *alarm) {
    int key = alarm->hour * 60 + alarm->minute + (alarm->snoozed ? 100000 : 0);
    if (mode == OVERLAY_RINGING && key == shown_key) {
        return;
    }
    shown_key = key;
    if (!root) {
        create();
    }
    mode = OVERLAY_RINGING;
    lv_label_set_text(title_label, alarm->snoozed ? "Snoozed alarm" : "Alarm");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 84);
    set_time_text(alarm);
    lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(touch_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(touch_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(knob_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(knob_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(root);
}

static void show_confirm(const hg_alarm_snapshot_t *alarm) {
    if (!root) {
        create();
    }
    mode = OVERLAY_CONFIRM;
    confirm_until_ms = lv_tick_get() + HG_ALARM_CONFIRM_MS;
    lv_label_set_text(title_label, alarm->snoozed ? "Snoozed until" :
                                   alarm->enabled ? "Next alarm" : "Alarm off");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 84);
    if (alarm->enabled) {
        set_time_text(alarm);
        lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(touch_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(touch_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(knob_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(knob_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(root);
}

void hg_overlays_init(void) {
    root = icon_label = title_label = time_label = NULL;
    touch_icon = touch_text = knob_icon = knob_text = NULL;
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
