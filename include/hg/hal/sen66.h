#ifndef HG_HAL_SEN66_H
#define HG_HAL_SEN66_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;
    float temp_c;
    float humidity;
    uint16_t voc_index;
    uint16_t nox_index;
    float pm2_5;
} hg_sen66_sample_t;

bool hg_sen66_init(void);
bool hg_sen66_read(hg_sen66_sample_t *out);

#endif
