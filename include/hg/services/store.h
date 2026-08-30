#ifndef HG_SERVICES_STORE_H
#define HG_SERVICES_STORE_H

#include <stdbool.h>

#include "hg/types.h"

void hg_store_init(void);
bool hg_store_load_alarm(hg_alarm_snapshot_t *out);
bool hg_store_save_alarm(const hg_alarm_snapshot_t *in);

#endif
