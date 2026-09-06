#include "hg/ui/boot_overlay.h"

#include <lvgl.h>

#include "hg/ui/fonts.h"
#include "hg/ui/theme.h"

static bool active = false;
static lv_obj_t *overlay;
static lv_obj_t *icon_label;
static lv_obj_t *status_label;
static uint8_t dot_phase = 0;
static uint8_t icon_phase = 0;

static const char *const wifi_icon_frames[] = {
    "\uF0B0",  // 0 bars
    "\uEBE4",  // 1 bar
    "\uEBD6",  // 2 bars
    "\uEBE1",  // 3 bars
    "\uE1D8",  // 4 bars
};

static void hg_boot_overlay_set_status_connecting(const char *ssid) {
    static char status_text[96];
    const char *target_ssid = (ssid && ssid[0] != '\0') ? ssid : "Wi-Fi";

    if (dot_phase == 0) {
        lv_snprintf(status_text, sizeof(status_text), "Connecting to %s.", target_ssid);
    } else if (dot_phase == 1) {
        lv_snprintf(status_text, sizeof(status_text), "Connecting to %s..", target_ssid);
    } else {
        lv_snprintf(status_text, sizeof(status_text), "Connecting to %s...", target_ssid);
    }

    lv_label_set_text(status_label, status_text);
    dot_phase = (dot_phase + 1U) % 3U;
}

static void hg_boot_overlay_set_status_syncing_time(void) {
    static char status_text[48];

    if (dot_phase == 0) {
        lv_snprintf(status_text, sizeof(status_text), "Syncing time.");
    } else if (dot_phase == 1) {
        lv_snprintf(status_text, sizeof(status_text), "Syncing time..");
    } else {
        lv_snprintf(status_text, sizeof(status_text), "Syncing time...");
    }

    lv_label_set_text(status_label, status_text);
    dot_phase = (dot_phase + 1U) % 3U;
}

static void hg_boot_overlay_set_icon_frame(void) {
    lv_label_set_text(icon_label, wifi_icon_frames[icon_phase]);
    icon_phase = (icon_phase + 1U) % (sizeof(wifi_icon_frames) / sizeof(wifi_icon_frames[0]));
}

static void hg_boot_overlay_show(void) {
    if (active) {
        return;
    }

    overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, hg_theme_bg(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_move_foreground(overlay);

    icon_label = lv_label_create(overlay);
    lv_obj_set_style_text_font(icon_label, HG_FONT_WIFI_ICON, 0);
    lv_obj_set_style_text_color(icon_label, hg_theme_fg(), 0);
    lv_label_set_text(icon_label, wifi_icon_frames[0]);
    lv_obj_set_style_text_align(icon_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(icon_label, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(icon_label, LV_ALIGN_CENTER, 0, -24);

    status_label = lv_label_create(overlay);
    lv_obj_set_style_text_font(status_label, HG_FONT_STATUS, 0);
    lv_obj_set_style_text_color(status_label, hg_theme_fg(), 0);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(status_label, LV_PCT(90));
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(status_label, icon_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 18);

    dot_phase = 0;
    icon_phase = 0;
    active = true;
}

void hg_boot_overlay_create(void) {
    overlay = NULL;
    icon_label = NULL;
    status_label = NULL;
    dot_phase = 0;
    icon_phase = 0;
    active = false;
}

void hg_boot_overlay_update(
    const hg_wifi_snapshot_t *wifi,
    const hg_time_snapshot_t *time,
    const char *ssid) {
    if (!wifi || !time) {
        return;
    }

    bool wifi_ready = (wifi->state == HG_WIFI_CONNECTED);
    bool time_ready = time->valid;

    if (wifi_ready && time_ready) {
        hg_boot_overlay_destroy();
        return;
    }

    if (!active) {
        hg_boot_overlay_show();
    }

    if (overlay) {
        lv_obj_move_foreground(overlay);
        lv_obj_set_style_bg_color(overlay, hg_theme_bg(), 0);
    }

    if (icon_label) {
        hg_boot_overlay_set_icon_frame();
        lv_obj_set_style_text_color(icon_label, hg_theme_fg(), 0);
    }

    if (status_label) {
        lv_obj_set_style_text_color(status_label, hg_theme_fg(), 0);
        if (!wifi_ready) {
            hg_boot_overlay_set_status_connecting(ssid);
        } else {
            hg_boot_overlay_set_status_syncing_time();
        }
    }
}

void hg_boot_overlay_destroy(void) {
    if (overlay) {
        lv_obj_del(overlay);
    }

    overlay = NULL;
    icon_label = NULL;
    status_label = NULL;
    dot_phase = 0;
    icon_phase = 0;
    active = false;
}

bool hg_boot_overlay_is_active(void) {
    return active;
}
