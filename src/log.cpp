#include "hg/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

static const size_t line_size = 128;
static StaticQueue_t queue_state;
static uint8_t queue_storage[16 * line_size];
static QueueHandle_t lines = nullptr;
static portMUX_TYPE dropped_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t dropped = 0;

void hg_log_init(void) {
    lines = xQueueCreateStatic(16, line_size, queue_storage, &queue_state);
    configASSERT(lines);
}

void hg_log_write(const char *tag, const char *format, ...) {
    char line[line_size] = {};
    int prefix = snprintf(line, sizeof(line), "[%8lu %-8s] ",
                          (unsigned long)millis(), tag);
    if (prefix < 0 || (size_t)prefix >= sizeof(line)) {
        prefix = 0;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(line + prefix, sizeof(line) - prefix, format, args);
    va_end(args);
    if (!lines || xQueueSend(lines, line, 0) != pdTRUE) {
        portENTER_CRITICAL(&dropped_mux);
        ++dropped;
        portEXIT_CRITICAL(&dropped_mux);
    }
}

void hg_log_flush(void) {
    static char pending[line_size] = {};
    static size_t offset = 0;
    static size_t length = 0;
    if (offset == length) {
        if (!lines || xQueueReceive(lines, pending, 0) != pdTRUE) {
            return;
        }
        offset = 0;
        length = strnlen(pending, sizeof(pending));
    }
    int available = Serial.availableForWrite();
    if (available > 0 && offset < length) {
        size_t count = length - offset;
        if (count > (size_t)available) {
            count = (size_t)available;
        }
        offset += Serial.write((const uint8_t *)pending + offset, count);
    }
}

uint32_t hg_log_dropped(void) {
    portENTER_CRITICAL(&dropped_mux);
    uint32_t result = dropped;
    portEXIT_CRITICAL(&dropped_mux);
    return result;
}
