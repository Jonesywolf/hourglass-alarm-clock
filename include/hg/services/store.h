#ifndef HG_SERVICES_STORE_H
#define HG_SERVICES_STORE_H

#include <stdbool.h>
#include <stddef.h>

#include "hg/types.h"

void hg_store_init(void);
bool hg_store_load_alarm(hg_alarm_snapshot_t *out);
bool hg_store_start_worker(void);
void hg_store_request_save_alarm(const hg_alarm_snapshot_t *in);
void hg_store_set_flash_allowed(bool allowed);

typedef struct {
    uint32_t requested_generation;
    uint32_t saved_generation;
    uint32_t failures;
    uint32_t stack_free_bytes;
    uint32_t last_write_us;
    uint32_t successful_saves;
    bool pending;
    bool writing;
} hg_store_status_t;

void hg_store_get_status(hg_store_status_t *out);

#ifdef HG_RUNTIME_TEST
void hg_store_test_writer(uint32_t delay_ms, bool fail);
void hg_store_test_scratch(bool enabled);
bool hg_store_test_decode_alarm(const uint8_t *bytes, size_t size, hg_alarm_snapshot_t *out);
void hg_store_test_get_desired(hg_alarm_snapshot_t *out);
#endif

#endif
