#include "hg/ui/theme.h"

static bool nightlight = false;

void hg_theme_init(void) {
    nightlight = false;
}

void hg_theme_set_nightlight(bool enabled) {
    nightlight = enabled;
}

lv_color_t hg_theme_fg(void) {
    return nightlight ? lv_color_hex(0xFF3030) : lv_color_white();
}

lv_color_t hg_theme_bg(void) {
    return lv_color_black();
}

lv_color_t hg_theme_muted(void) {
    return nightlight ? lv_color_hex(0x7A2020) : lv_color_hex(0x666666);
}

lv_color_t hg_theme_alarm(void) {
    return nightlight ? lv_color_hex(0x7A2020) : lv_color_hex(0xB0B0B0);
}
