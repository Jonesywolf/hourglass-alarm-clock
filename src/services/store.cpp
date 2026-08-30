#include "hg/services/store.h"

void hg_store_init(void) {}

bool hg_store_load_alarm(hg_alarm_snapshot_t *out) {
    (void)out;
    return false;
}

bool hg_store_save_alarm(const hg_alarm_snapshot_t *in) {
    (void)in;
    return false;
}
