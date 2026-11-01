#ifndef HG_SERVICES_HTTP_INTERNAL_H
#define HG_SERVICES_HTTP_INTERNAL_H

#include <ArduinoJson.h>

extern const char *const hg_http_root_cas;

/* Blocking HTTPS GET verified against hg_http_root_cas. Parses the body through `filter`.
 * Weather worker only: never call from the UI task. Logs and returns false on any failure. */
bool hg_http_get_json(const char *url, JsonDocument &doc, const JsonDocument &filter);

#endif
