#ifndef LVCONF_H
#define LVCONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16          // 16-bit RGB565 (standard for most SPI/IPS screens)
#define LV_COLOR_16_SWAP 0         // Set to 1 if display colors look inverted/blue-shifted

/* Memory Settings */
#define LV_MEM_CUSTOM 0            // Use LVGL's internal memory manager
#define LV_MEM_SIZE (64U * 1024U)   // 64 KB heap for UI widgets

/* Tick Customization */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis()) // Automatic tick source
#endif

/* Fonts enabled */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_28 1

#endif /* LV_CONF_H */