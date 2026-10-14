#include "hg/hal/display.h"

#include <Arduino.h>
#include <display/Arduino_ST7789.h>
#include <lvgl.h>

#include "hg/config.h"
#include "hg/hal/pins.h"
#include "display_bus_internal.h"

static_assert(LV_COLOR_16_SWAP == 1 && sizeof(lv_color_t) == 2, "SPI DMA requires wire-order RGB565");

static hg_display_bus_t bus;
static Arduino_ST7789 gfx(&bus, HG_PIN_TFT_RST, 1, true, 240, 320);
static lv_disp_draw_buf_t draw_buf;
DMA_ATTR static lv_color_t buffers[2][HG_DISP_HOR_RES * HG_DISP_DRAW_BUF_LINES];
static lv_disp_drv_t *pending = nullptr;
static bool pending_last = false;
static bool pending_measured = false;
static uint32_t pending_started_us = 0;
static uint32_t pending_pixels = 0;
static hg_timing_stats_t frame_timing;
static bool measure_frames = false;
static uint32_t last_frame_us = 0;
static hg_display_transfer_timing_t transfer_timing;
static uint32_t frame_transfer_us = 0;
static uint32_t frame_pixels = 0;

static void complete_flush(bool wait) {
    uint32_t completed_us = 0;
    if (!pending || !bus.finish(wait, &completed_us)) {
        return;
    }
    gfx.endWrite();
    if (pending_measured) {
        frame_transfer_us += completed_us - pending_started_us;
        frame_pixels += pending_pixels;
        if (pending_last) {
            if (frame_transfer_us > transfer_timing.max_frame_transfer_us) {
                transfer_timing.max_frame_transfer_us = frame_transfer_us;
            }
            if (frame_pixels > transfer_timing.max_frame_pixels) {
                transfer_timing.max_frame_pixels = frame_pixels;
            }
            if (last_frame_us) {
                hg_timing_record(&frame_timing, completed_us - last_frame_us);
            }
            last_frame_us = measure_frames ? completed_us : 0;
            frame_transfer_us = 0;
            frame_pixels = 0;
        }
    }
    lv_disp_flush_ready(pending);
    pending = nullptr;
}

static void hg_display_wait(lv_disp_drv_t *) {
    complete_flush(true);
}

static void hg_display_round(lv_disp_drv_t *, lv_area_t *area) {
    // Word-aligned DMA rectangles avoid per-transfer bounce allocations.
    area->x1 &= ~1;
    area->x2 |= 1;
}

static void hg_display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    configASSERT(!pending);
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    pending = disp;
    pending_pixels = width * height;
    pending_last = lv_disp_flush_is_last(disp);
    pending_measured = measure_frames;
    gfx.startWrite();
    gfx.writeAddrWindow(area->x1, area->y1, width, height);
    pending_started_us = micros();
    bus.queue_pixels((const uint8_t *)color_p, pending_pixels * sizeof(lv_color_t));
}

bool hg_display_init(void) {
    if (!gfx.begin()) {
        return false;
    }
    gfx.setRotation(1);
    gfx.fillScreen(BLACK);
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buffers[0], buffers[1],
                         HG_DISP_HOR_RES * HG_DISP_DRAW_BUF_LINES);
    static lv_disp_drv_t driver;
    lv_disp_drv_init(&driver);
    driver.hor_res = HG_DISP_HOR_RES;
    driver.ver_res = HG_DISP_VER_RES;
    driver.flush_cb = hg_display_flush;
    driver.wait_cb = hg_display_wait;
    driver.rounder_cb = hg_display_round;
    driver.draw_buf = &draw_buf;
    return lv_disp_drv_register(&driver) != nullptr;
}

void hg_display_set_brightness(uint8_t percent) {
    (void)percent;
}

void hg_display_poll(void) {
    complete_flush(false);
}

void hg_display_measure_frames(bool active) {
    measure_frames = active;
    if (!active && !pending) {
        last_frame_us = 0;
        frame_transfer_us = 0;
        frame_pixels = 0;
    }
}

void hg_display_get_frame_timing(hg_timing_stats_t *out) {
    *out = frame_timing;
}

void hg_display_reset_frame_timing(void) {
    complete_flush(true);
    frame_timing = {};
    last_frame_us = 0;
    frame_transfer_us = 0;
    frame_pixels = 0;
    transfer_timing = {};
}

void hg_display_get_transfer_timing(hg_display_transfer_timing_t *out) {
    *out = transfer_timing;
}
