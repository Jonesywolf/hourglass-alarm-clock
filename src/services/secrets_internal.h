#ifndef HG_SERVICES_SECRETS_INTERNAL_H
#define HG_SERVICES_SECRETS_INTERNAL_H

#include <stdbool.h>

/* Only src/services/secrets.cpp includes secrets.h (it defines non-static globals). */
const char *hg_secret_wifi_ssid(void);
const char *hg_secret_wifi_password(void);
/* True when secrets.h sets WEATHER_LATITUDE/WEATHER_LONGITUDE; name may be "" if unset. */
bool hg_secret_location(float *lat, float *lon, const char **name);

#endif
