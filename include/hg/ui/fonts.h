#ifndef HG_UI_FONTS_H
#define HG_UI_FONTS_H

#include <lvgl.h>

/* Generated C fonts under src/ui/assets/fonts/ come from tools/fonts/fonts.json
 * (platformio run -t fonts). Never include those .c files; add LV_FONT_DECLARE() here. */

LV_FONT_DECLARE(icons_72);
LV_FONT_DECLARE(icons_14);
LV_FONT_DECLARE(icons_36);
LV_FONT_DECLARE(icons_alarm_72);
LV_FONT_DECLARE(inter_96);
LV_FONT_DECLARE(inter_28);
LV_FONT_DECLARE(inter_14);

#define HG_FONT_CLOCK   (&inter_96)
#define HG_FONT_STATUS  (&inter_14)
#define HG_FONT_ALARM   (&inter_28)
#define HG_FONT_WIFI_ICON  (&icons_72)
#define HG_FONT_WIFI_ICON_SMALL  (&icons_14)
#define HG_FONT_ALARM_ICON  (&icons_alarm_72)
#define HG_FONT_ACTION_ICON  (&icons_36)

/* Glyphs in icons_alarm_72 / icons_36 (Material Icons Outlined). */
#define HG_ICON_ALARM      "\uE855"
#define HG_ICON_SNOOZE     "\uE046"
#define HG_ICON_ALARM_OFF  "\uE857"

#endif
