#ifndef HG_UI_BOOT_OVERLAY_H
#define HG_UI_BOOT_OVERLAY_H

#include <stdbool.h>

#include "hg/types.h"

void hg_boot_overlay_create(void);
void hg_boot_overlay_update(
	const hg_wifi_snapshot_t *wifi,
	const hg_time_snapshot_t *time,
	const char *ssid);
void hg_boot_overlay_destroy(void);
bool hg_boot_overlay_is_active(void);

#endif
