#include "secrets_internal.h"

#include "secrets.h"

#ifndef WEATHER_LOCATION_NAME
#define WEATHER_LOCATION_NAME ""
#endif

const char *hg_secret_wifi_ssid(void) {
    return WIFI_SSID;
}

const char *hg_secret_wifi_password(void) {
    return WIFI_PASSWORD;
}

bool hg_secret_location(float *lat, float *lon, const char **name) {
#if defined(WEATHER_LATITUDE) && defined(WEATHER_LONGITUDE)
    *lat = (float)(WEATHER_LATITUDE);
    *lon = (float)(WEATHER_LONGITUDE);
    *name = WEATHER_LOCATION_NAME;
    return true;
#else
    (void)lat;
    (void)lon;
    *name = "";
    return false;
#endif
}
