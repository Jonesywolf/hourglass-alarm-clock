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


bool hg_theme_nightlight(void) {
    return nightlight;
}

lv_color_t hg_theme_signal(lv_color_t color) {
    if (!nightlight) {
        return color;
    }
    uint32_t c32 = lv_color_to32(color);
    uint32_t r = (c32 >> 16) & 0xFF, g = (c32 >> 8) & 0xFF, b = c32 & 0xFF;
    uint32_t luminance = (r * 54 + g * 183 + b * 19) >> 8;
    return hg_theme_tone((uint8_t)(luminance < 0x60 ? 0x60 : luminance));
}

lv_color_t hg_theme_dim(void) {
    return hg_theme_tone(0x2A);
}
