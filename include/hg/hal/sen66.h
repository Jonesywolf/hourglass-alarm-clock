#ifndef HG_HAL_SEN66_H
#define HG_HAL_SEN66_H

#include <stdbool.h>
#include <stdint.h>

#include "hg/types.h"

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

void hg_sen66_init(void);
/* READY means that out contains a new frame; WAITING does not replace the last frame. */
hg_sample_status_t hg_sen66_update(uint32_t now_ms, hg_sen66_sample_t *out);
void hg_sen66_set_paused(bool paused, uint32_t now_ms);

#ifdef HG_RUNTIME_TEST
typedef uint16_t (*hg_sen66_test_send_t)(uint16_t command);
typedef uint16_t (*hg_sen66_test_receive_t)(uint8_t *payload, uint8_t words);
void hg_sen66_test_transport(hg_sen66_test_send_t send, hg_sen66_test_receive_t receive);
void hg_sen66_test_force_error(bool enabled);
#endif

#endif