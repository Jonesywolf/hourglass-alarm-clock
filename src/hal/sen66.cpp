#include "hg/hal/sen66.h"

/* Driver not wired yet: report unavailable so the indoor service stays UNAVAILABLE. */
bool hg_sen66_init(void) {
    return false;
}

bool hg_sen66_read(hg_sen66_sample_t *out) {
    if (out) {
        *out = {};
    }
    return false;
}