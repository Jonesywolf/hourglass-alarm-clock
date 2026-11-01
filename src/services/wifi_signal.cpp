#include "hg/services/wifi_signal.h"

hg_wifi_bars_t hg_wifi_bars_from_rssi(int rssi) {
    if (rssi >= -65) {
        return HG_WIFI_BARS_HIGH;
    }
    if (rssi >= -75) {
        return HG_WIFI_BARS_MEDIUM;
    }
    if (rssi >= -85) {
        return HG_WIFI_BARS_LOW;
    }
    return HG_WIFI_BARS_NONE;
}
