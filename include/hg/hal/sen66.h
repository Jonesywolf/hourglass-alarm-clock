#ifndef HG_HAL_SEN66_H
#define HG_HAL_SEN66_H

#include <stdbool.h>
#include <stdint.h>

/* Sensirion SEN66 measured values. VOC/NOx are Sensirion index values (1-500); CO2 in ppm. */
typedef struct {
    bool valid;
    float temp_c;
    float humidity;
    float pm1_0;
    float pm2_5;
    float pm4_0;
    float pm10;
    uint16_t co2_ppm;
    uint16_t voc_index;
    uint16_t nox_index;
} hg_sen66_sample_t;

bool hg_sen66_init(void);
bool hg_sen66_read(hg_sen66_sample_t *out);

#endif