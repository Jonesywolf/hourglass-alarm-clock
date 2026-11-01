#include "http_internal.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "hg/config.h"
#include "hg/log.h"

bool hg_http_get_json(const char *url, JsonDocument &doc, const JsonDocument &filter) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    WiFiClientSecure client;
    client.setCACert(hg_http_root_cas);
    client.setHandshakeTimeout((HG_WEATHER_HTTP_TIMEOUT_MS + 999U) / 1000U);
    HTTPClient http;
    /* HTTP/1.0 avoids chunked bodies so ArduinoJson can parse the stream directly. */
    http.useHTTP10(true);
    http.setConnectTimeout(HG_WEATHER_HTTP_TIMEOUT_MS);
    http.setTimeout(HG_WEATHER_HTTP_TIMEOUT_MS);
    http.setUserAgent("hourglass-alarm-clock");
    if (!http.begin(client, url)) {
        HG_ERROR("http", "Bad URL\n");
        return false;
    }
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        HG_ERROR("http", "GET failed: %d\n", code);
        http.end();
        return false;
    }
    DeserializationError error =
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (error) {
        HG_ERROR("http", "JSON error: %s\n", error.c_str());
        return false;
    }
    return true;
}
