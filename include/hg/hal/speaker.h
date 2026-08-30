#ifndef HG_HAL_SPEAKER_H
#define HG_HAL_SPEAKER_H

#include <stdbool.h>

typedef enum {
    HG_SPEAKER_STOP = 0,
    HG_SPEAKER_SOFT,
    HG_SPEAKER_HARD
} hg_speaker_mode_t;

bool hg_speaker_init(void);
void hg_speaker_set_mode(hg_speaker_mode_t mode);

#endif
