#include "hg/hal/display.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include "hg/config.h"
#include "hg/hal/pins.h"

static Arduino_DataBus *bus = nullptr;
static Arduino_GFX *gfx = nullptr;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[HG_DISP_HOR_RES * HG_DISP_DRAW_BUF_LINES];

static void hg_display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

    lv_disp_flush_ready(disp);
}

bool hg_display_init(void) {
    bus = new Arduino_ESP32SPI(
        HG_PIN_TFT_DC, HG_PIN_TFT_CS, HG_PIN_TFT_SCK, HG_PIN_TFT_MOSI, GFX_NOT_DEFINED);
    gfx = new Arduino_ST7789(bus, HG_PIN_TFT_RST, 1 /* Landscape */, true /* IPS */, 240, 320);

    gfx->begin();
    gfx->setRotation(1);
    gfx->fillScreen(BLACK);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, HG_DISP_HOR_RES * HG_DISP_DRAW_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = HG_DISP_HOR_RES;
    disp_drv.ver_res = HG_DISP_VER_RES;
    disp_drv.flush_cb = hg_display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    return true;
}

void hg_display_set_brightness(uint8_t percent) {
    (void)percent;
}
