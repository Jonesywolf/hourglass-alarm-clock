#ifndef HG_HAL_TOUCH_H
#define HG_HAL_TOUCH_H

#include <stdbool.h>

bool hg_touch_init(void);
void hg_touch_update(void);
bool hg_touch_is_pressed(void);
bool hg_touch_tapped(void);

#endif
