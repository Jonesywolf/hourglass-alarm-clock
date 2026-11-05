#include "hg/ui/theme.h"

static bool nightlight = false;

void hg_theme_init(void) {
    nightlight = false;
}

void hg_theme_set_nightlight(bool enabled) {
    nightlight = enabled;
}

/* Nightlight ramp: warm red-to-amber with no blue. Hierarchy comes from hue as well as
 * brightness, so every text tone (muted and up) still meets WCAG 2.2 AA 4.5:1 on black. */
typedef struct {
    uint8_t v, r, g, b;
} night_stop_t;

static const night_stop_t night_ramp[] = {
    {0x00, 0x00, 0x00, 0x00},
    {0x2A, 0x60, 0x10, 0x00}, /* dim: decorative tracks only */
    {0x7A, 0xE0, 0x38, 0x0C}, /* muted: 5.0:1 */
    {0xB0, 0xF4, 0x44, 0x10}, /* alarm: 6.0:1 */
    {0xFF, 0xFF, 0x58, 0x18}, /* fg: 6.7:1 */
};

static uint8_t lerp(uint8_t a, uint8_t b, uint8_t t, uint8_t span) {
    return (uint8_t)(a + ((int)b - a) * t / span);
}

static lv_color_t hg_theme_tone(uint8_t v) {
    if (!nightlight) {
        return lv_color_make(v, v, v);
    }
    unsigned i = 1;
    while (i < sizeof(night_ramp) / sizeof(night_ramp[0]) - 1 && v > night_ramp[i].v) {
        ++i;
    }
    const night_stop_t *lo = &night_ramp[i - 1], *hi = &night_ramp[i];
    uint8_t t = v - lo->v, span = hi->v - lo->v;
    return lv_color_make(lerp(lo->r, hi->r, t, span), lerp(lo->g, hi->g, t, span),
                         lerp(lo->b, hi->b, t, span));
}

lv_color_t hg_theme_fg(void) {
    return hg_theme_tone(0xFF);
}

lv_color_t hg_theme_bg(void) {
    return lv_color_black();
}

/* Lowest tone allowed for text: 0x7A grey is 4.9:1 on black (WCAG 2.2 AA). */
lv_color_t hg_theme_muted(void) {
    return hg_theme_tone(0x7A);
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
    return hg_theme_tone((uint8_t)(luminance < 0x7A ? 0x7A : luminance));
}

lv_color_t hg_theme_dim(void) {
    return hg_theme_tone(0x2A);
}
