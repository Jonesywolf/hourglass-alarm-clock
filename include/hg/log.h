#ifndef HG_LOG_H
#define HG_LOG_H

#include "hg/config.h"
#include <stdint.h>

#include <Arduino.h>

void hg_log_init(void);
void hg_log_write(const char *tag, const char *format, ...);
void hg_log_flush(void);
uint32_t hg_log_dropped(void);

#define HG_ERROR(tag, fmt, ...) hg_log_write((tag), (fmt), ##__VA_ARGS__)

#if HG_LOG_TOUCH_DEBUG
#define HG_TOUCH_LOG(tag, fmt, ...) hg_log_write((tag), (fmt), ##__VA_ARGS__)
#else
#define HG_TOUCH_LOG(tag, fmt, ...) ((void)0)
#endif

#if HG_LOG_DEBUG
#define HG_LOG(tag, fmt, ...) \
    hg_log_write((tag), (fmt), ##__VA_ARGS__)

#define HG_LOG_TASK(tag, stmt) \
    do { \
        uint32_t _hg_t0 = millis(); \
        stmt; \
        HG_LOG((tag), "ran %lu ms\n", (unsigned long)(millis() - _hg_t0)); \
    } while (0)

/* For work that runs every loop: only print if it took at least min_ms. */
#define HG_LOG_TASK_IF(tag, min_ms, stmt) \
    do { \
        uint32_t _hg_t0 = millis(); \
        stmt; \
        uint32_t _hg_dt = millis() - _hg_t0; \
        if (_hg_dt >= (uint32_t)(min_ms)) { \
            HG_LOG((tag), "ran %lu ms\n", (unsigned long)_hg_dt); \
        } \
    } while (0)
#else
#define HG_LOG(tag, fmt, ...) ((void)0)
#define HG_LOG_TASK(tag, stmt) \
    do { \
        stmt; \
    } while (0)
#define HG_LOG_TASK_IF(tag, min_ms, stmt) \
    do { \
        stmt; \
    } while (0)
#endif

#endif
