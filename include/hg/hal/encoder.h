#ifndef HG_HAL_ENCODER_H
#define HG_HAL_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

bool hg_encoder_init(void);
void hg_encoder_update(void);
int32_t hg_encoder_take_delta(void);
bool hg_encoder_button_pressed(void);

#endif
