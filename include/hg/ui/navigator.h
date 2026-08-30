#ifndef HG_UI_NAVIGATOR_H
#define HG_UI_NAVIGATOR_H

#include "hg/types.h"

void hg_navigator_init(void);
void hg_navigator_show_page(hg_page_id_t page);
void hg_navigator_next(void);
void hg_navigator_prev(void);
hg_page_id_t hg_navigator_current(void);

#endif
