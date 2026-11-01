#ifndef HG_SERVICES_LOCATION_H
#define HG_SERVICES_LOCATION_H

#include <stdbool.h>
#include <stdint.h>

#include "hg/types.h"

typedef enum {
    HG_LOCATION_NONE = 0,
    HG_LOCATION_CACHED,
    HG_LOCATION_IP,
    HG_LOCATION_OVERRIDE
} hg_location_source_t;

typedef struct {
    bool valid;
    float latitude;
    float longitude;
    char name[HG_LOCATION_NAME_LEN];
    hg_location_source_t source;
} hg_location_t;

/* Called from app setup before workers start, with the NVS-cached location (may be invalid). */
void hg_location_seed(const hg_location_t *cached);
/* Blocking network calls; only the weather worker calls init/update. */
void hg_location_init(void);
/* Refreshes IP geolocation once per boot and then every HG_LOCATION_REFRESH_MS. */
void hg_location_update(void);
void hg_location_get(hg_location_t *out);

#endif
