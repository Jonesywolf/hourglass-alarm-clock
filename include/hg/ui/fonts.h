#ifndef HG_UI_FONTS_H
#define HG_UI_FONTS_H

#include <lvgl.h>

/* Generated C fonts under src/ui/assets/fonts/ come from tools/fonts/fonts.json
 * (platformio run -t fonts). Never include those .c files; add LV_FONT_DECLARE() here. */

LV_FONT_DECLARE(inter_96);
LV_FONT_DECLARE(inter_48);
LV_FONT_DECLARE(inter_28);
LV_FONT_DECLARE(inter_14);

#define HG_FONT_CLOCK   (&inter_96)
#define HG_FONT_STATUS  (&inter_14)
#define HG_FONT_ALARM   (&inter_28)
#define HG_FONT_ALARM_TIME (&inter_48)
#define HG_FONT_WEATHER_TEMP (&inter_48)

#endif
