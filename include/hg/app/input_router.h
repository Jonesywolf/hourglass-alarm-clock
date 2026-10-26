#ifndef HG_APP_INPUT_ROUTER_H
#define HG_APP_INPUT_ROUTER_H

#include <stdbool.h>
#include "hg/types.h"

void hg_input_router_init(void);
bool hg_input_router_update(bool navigation_allowed);
void hg_input_router_get_alarm_edit(hg_alarm_edit_snapshot_t *out);
void hg_input_router_set_volume_allowed(bool allowed);
void hg_input_router_get_volume(hg_volume_snapshot_t *out);

#ifdef HG_RUNTIME_TEST
bool hg_input_router_test_update(bool navigation_allowed, uint32_t now_ms);
#endif

#endif
