#include "hg/ui/clock_screen.h"

#include "hg/ui/fonts.h"
#include "hg/ui/theme.h"

static lv_obj_t *clock_label;
static lv_obj_t *meridiem_label;
static lv_obj_t *wifi_label;
static lv_obj_t *alarm_label;
static hg_wifi_bars_t displayed_wifi_bars = HG_WIFI_BARS_NONE;
static hg_wifi_bars_t pending_wifi_bars = HG_WIFI_BARS_NONE;
static uint8_t pending_wifi_samples = 0;

static const char *const wifi_icon_glyphs[] = {
    "\uF0B0",  // 0 bars
    "\uEBE4",  // 1 bar
    "\uEBD6",  // 2 bars
    "\uEBE1",  // 3 bars
    "\uE1D8",  // 4 bars
};

static hg_wifi_bars_t hg_clock_clamp_wifi_bars(hg_wifi_bars_t bars) {
    if (bars > HG_WIFI_BARS_4) {
        return HG_WIFI_BARS_4;
    }
    return bars;
}

static void hg_clock_set_wifi_icon(hg_wifi_bars_t bars, bool force) {
    hg_wifi_bars_t clamped_bars = hg_clock_clamp_wifi_bars(bars);

    if (!wifi_label) {
        return;
    }

    if (!force && clamped_bars == displayed_wifi_bars) {
        return;
    }

    displayed_wifi_bars = clamped_bars;
    lv_label_set_text(wifi_label, wifi_icon_glyphs[clamped_bars]);
}

static void hg_clock_update_meridiem_position(void) {
    if (!clock_label || !meridiem_label) {
        return;
    }

    lv_obj_t *parent = lv_obj_get_parent(clock_label);
    if (parent) {
        lv_obj_update_layout(parent);
    }

    lv_coord_t clock_w = lv_obj_get_width(clock_label);
    lv_coord_t meridiem_y = lv_obj_get_y(clock_label);
    if (lv_label_get_text(meridiem_label)[0] == 'P') {
        meridiem_y += lv_obj_get_height(clock_label) - lv_obj_get_height(meridiem_label);
    }
    lv_obj_set_pos(meridiem_label, lv_obj_get_x(clock_label) + clock_w + 4, meridiem_y);
}

void hg_clock_screen_create(lv_obj_t *parent) {
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, hg_theme_bg(), 0);

    wifi_label = lv_label_create(parent);
    lv_obj_set_style_text_font(wifi_label, HG_FONT_WIFI_ICON_SMALL, 0);
    lv_obj_set_style_text_color(wifi_label, hg_theme_fg(), 0);
    displayed_wifi_bars = HG_WIFI_BARS_NONE;
    pending_wifi_bars = HG_WIFI_BARS_NONE;
    pending_wifi_samples = 0;
    hg_clock_set_wifi_icon(HG_WIFI_BARS_NONE, true);
    lv_obj_align(wifi_label, LV_ALIGN_TOP_RIGHT, -10, 14);

    clock_label = lv_label_create(parent);
    lv_obj_set_style_text_font(clock_label, HG_FONT_CLOCK, 0);
    lv_obj_set_style_text_color(clock_label, hg_theme_fg(), 0);
    lv_label_set_text(clock_label, "--:--");
    lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, 0);

    meridiem_label = lv_label_create(parent);
    lv_obj_set_style_text_font(meridiem_label, HG_FONT_STATUS, 0);
    lv_obj_set_style_text_color(meridiem_label, hg_theme_fg(), 0);
    lv_label_set_text(meridiem_label, "AM");
    hg_clock_update_meridiem_position();

    alarm_label = lv_label_create(parent);
    lv_obj_set_style_text_font(alarm_label, HG_FONT_ALARM, 0);
    lv_obj_set_style_text_color(alarm_label, hg_theme_alarm(), 0);
    lv_label_set_text(alarm_label, "--:--");
    lv_obj_align(alarm_label, LV_ALIGN_BOTTOM_LEFT, 10, -10);
}

void hg_clock_screen_update_alarm(const hg_alarm_snapshot_t *alarm) {
    if (!alarm || !alarm_label) {
        return;
    }

    char alarm_str[16];
    int alarm_hour12 = alarm->hour % 12;
    if (alarm_hour12 == 0) {
        alarm_hour12 = 12;
    }
    lv_snprintf(alarm_str, sizeof(alarm_str), "%d:%02d%s", alarm_hour12, alarm->minute,
                (alarm->hour >= 12) ? "pm" : "am");
    lv_label_set_text(alarm_label, alarm_str);
}

void hg_clock_screen_update(const hg_clock_snapshot_t *snap) {
    if (!snap || !clock_label) {
        return;
    }

    hg_clock_screen_update_alarm(&snap->alarm);

    if (!snap->time.valid) {
        lv_label_set_text(clock_label, "--:--");
        lv_label_set_text(meridiem_label, "--");
        hg_clock_update_meridiem_position();
        lv_obj_set_style_text_color(wifi_label, hg_theme_muted(), 0);
        pending_wifi_bars = HG_WIFI_BARS_NONE;
        pending_wifi_samples = 0;
        hg_clock_set_wifi_icon(HG_WIFI_BARS_NONE, false);
        return;
    }

    char time_str[16];
    int hour12 = snap->time.hour_24 % 12;
    if (hour12 == 0) {
        hour12 = 12;
    }
    lv_snprintf(time_str, sizeof(time_str), "%d:%02d", hour12, snap->time.minute);
    lv_label_set_text(clock_label, time_str);
    lv_label_set_text(meridiem_label, (snap->time.hour_24 >= 12) ? "PM" : "AM");
    hg_clock_update_meridiem_position();

    lv_obj_set_style_text_color(
        wifi_label,
        (snap->wifi.state == HG_WIFI_CONNECTED) ? hg_theme_fg() : hg_theme_muted(),
        0);

    if (snap->wifi.state != HG_WIFI_CONNECTED) {
        pending_wifi_bars = HG_WIFI_BARS_NONE;
        pending_wifi_samples = 0;
        hg_clock_set_wifi_icon(HG_WIFI_BARS_NONE, false);
        return;
    }

    hg_wifi_bars_t next_bars = hg_clock_clamp_wifi_bars(snap->wifi.bars);
    if (next_bars == displayed_wifi_bars) {
        pending_wifi_samples = 0;
        return;
    }

    if (next_bars != pending_wifi_bars) {
        pending_wifi_bars = next_bars;
        pending_wifi_samples = 1;
        return;
    }

    if (pending_wifi_samples < 255) {
        pending_wifi_samples++;
    }

    if (pending_wifi_samples >= 2) {
        pending_wifi_samples = 0;
        hg_clock_set_wifi_icon(pending_wifi_bars, false);
    }
}
