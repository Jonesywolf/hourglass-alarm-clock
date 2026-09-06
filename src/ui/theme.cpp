#include "hg/ui/theme.h"

static bool nightlight = false;

void hg_theme_init(void) {
    nightlight = false;
}

void hg_theme_set_nightlight(bool enabled) {
    nightlight = enabled;
}

/* Chroma scale tuned so fg's red (0xFF, 0x30, 0x30) matches the prior hand-picked hex. */
static lv_color_t hg_theme_tone(uint8_t v) {
    if (!nightlight) {
        return lv_color_make(v, v, v);
    }
    uint8_t chroma = (uint8_t)(((uint16_t)v * 0x30) / 0xFF);
    return lv_color_make(v, chroma, chroma);
}

lv_color_t hg_theme_fg(void) {
    return hg_theme_tone(0xFF);
}

lv_color_t hg_theme_bg(void) {
    return lv_color_black();
}

lv_color_t hg_theme_muted(void) {
    return hg_theme_tone(0x66);
}

lv_color_t hg_theme_alarm(void) {
    return hg_theme_tone(0xB0);
}

