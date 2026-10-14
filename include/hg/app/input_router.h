#ifndef HG_APP_INPUT_ROUTER_H
#define HG_APP_INPUT_ROUTER_H

void hg_input_router_init(void);
#include <stdbool.h>

bool hg_input_router_update(bool navigation_allowed);

#endif
