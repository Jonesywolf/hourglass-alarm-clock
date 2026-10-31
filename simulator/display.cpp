#include "hg/app/simulator.h"
#include "hg/config.h"
#include "hg/hal/display.h"

#include <emscripten.h>
#include <lvgl.h>

static lv_disp_draw_buf_t draw;
static lv_color_t buffers[2][HG_DISP_HOR_RES * HG_DISP_DRAW_BUF_LINES];
static uint32_t frames = 0;
static uint8_t brightness = 100;

EM_JS(void, copy_pixels, (int x, int y, int width, int height, const void *pixels), {
    Module.onPixels(x, y, width, height, HEAPU8.subarray(pixels, pixels + width * height * 2));
});

EM_JS(void, set_backlight, (int percent), {
    Module.onBacklight(percent);
});

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
    copy_pixels(area->x1, area->y1, area->x2 - area->x1 + 1,
                area->y2 - area->y1 + 1, pixels);
    if (lv_disp_flush_is_last(driver)) {
        ++frames;
    }
    lv_disp_flush_ready(driver);
}

bool hg_display_init(void) {
    lv_init();
    lv_disp_draw_buf_init(&draw, buffers[0], buffers[1],
                         HG_DISP_HOR_RES * HG_DISP_DRAW_BUF_LINES);
    static lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = HG_DISP_HOR_RES;
    driver.ver_res = HG_DISP_VER_RES;
    driver.draw_buf = &draw;
    driver.flush_cb = flush;
    return lv_disp_drv_register(&driver) != nullptr;
}

void hg_sim_render(void) { lv_refr_now(nullptr); }
uint32_t hg_sim_frames(void) { return frames; }
uint8_t hg_sim_brightness(void) { return brightness; }
void hg_display_poll(void) {}
void hg_display_set_brightness(uint8_t percent) {
    brightness = percent;
    set_backlight(percent);
}
void hg_display_measure_frames(bool) {}
void hg_display_get_frame_timing(hg_timing_stats_t *out) { *out = {}; }
void hg_display_reset_frame_timing(void) {}
void hg_display_get_transfer_timing(hg_display_transfer_timing_t *out) { *out = {}; }
