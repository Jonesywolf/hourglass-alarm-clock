#ifndef HG_UI_THEME_H
#define HG_UI_THEME_H

#include <lvgl.h>

#include <stdbool.h>

void hg_theme_init(void);
void hg_theme_set_nightlight(bool enabled);
lv_color_t hg_theme_fg(void);
lv_color_t hg_theme_bg(void);
lv_color_t hg_theme_muted(void);
lv_color_t hg_theme_alarm(void);

#endif
