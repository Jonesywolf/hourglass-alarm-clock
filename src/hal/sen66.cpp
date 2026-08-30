#include "hg/hal/sen66.h"

bool hg_sen66_init(void) {
    return false;
}

bool hg_sen66_read(hg_sen66_sample_t *out) {
    if (out) {
        out->valid = false;
        out->temp_c = 0.0f;
        out->humidity = 0.0f;
        out->voc_index = 0;
        out->nox_index = 0;
        out->pm2_5 = 0.0f;
    }
    return false;
}
