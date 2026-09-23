#ifndef HG_LOG_H
#define HG_LOG_H

#include "hg/config.h"

#if HG_LOG_DEBUG
#include <Arduino.h>

#define HG_LOG(tag, fmt, ...) \
    Serial.printf("[%8lu %-8s] " fmt, (unsigned long)millis(), (tag), ##__VA_ARGS__)

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
