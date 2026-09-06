#ifndef HG_SERVICES_DISPLAY_MODE_H
#define HG_SERVICES_DISPLAY_MODE_H

typedef enum {
    HG_DISPLAY_MODE_NORMAL = 0,
    HG_DISPLAY_MODE_RED,
    HG_DISPLAY_MODE_OFF
} hg_display_mode_t;

void hg_display_mode_init(void);
void hg_display_mode_update(void);
hg_display_mode_t hg_display_mode_get(void);

#endif
