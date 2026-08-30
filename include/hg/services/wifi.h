#ifndef HG_SERVICES_WIFI_H
#define HG_SERVICES_WIFI_H

#include "hg/types.h"

void hg_wifi_init(void);
void hg_wifi_update(void);
void hg_wifi_get_snapshot(hg_wifi_snapshot_t *out);
const char *hg_wifi_get_target_ssid(void);

#endif
