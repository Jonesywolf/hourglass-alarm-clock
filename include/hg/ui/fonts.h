#ifndef HG_UI_FONTS_H
#define HG_UI_FONTS_H

#include <lvgl.h>

/* Built-in LVGL fonts used today. Add LV_FONT_DECLARE() here for
 * generated C fonts under src/ui/assets/fonts/. Never include those .c files. */

LV_FONT_DECLARE(icons_72);
LV_FONT_DECLARE(icons_14);
LV_FONT_DECLARE(inter_96);

#define HG_FONT_CLOCK   (&inter_96)
#define HG_FONT_STATUS  (&lv_font_montserrat_14)
#define HG_FONT_WIFI_ICON  (&icons_72)
#define HG_FONT_WIFI_ICON_SMALL  (&icons_14)

#endif
