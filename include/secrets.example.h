#ifndef SECRETS_H
#define SECRETS_H

// Keep SSID at 32 chars or fewer (802.11 max) for predictable UI status formatting.
const char *WIFI_SSID = "your-ssid";
const char *WIFI_PASSWORD = "your-password";

// Optional weather location override. Without these, the clock uses IP geolocation
// (ipapi.co) once a day and caches the result. Name is shown on the weather page.
// #define WEATHER_LATITUDE 47.6062
// #define WEATHER_LONGITUDE -122.3321
// #define WEATHER_LOCATION_NAME "Seattle"

#endif
