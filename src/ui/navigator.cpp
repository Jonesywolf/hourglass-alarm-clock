#include "hg/ui/navigator.h"

static hg_page_id_t current_page = HG_PAGE_CLOCK;

void hg_navigator_init(void) {
    current_page = HG_PAGE_CLOCK;
}

void hg_navigator_show_page(hg_page_id_t page) {
    if (page < HG_PAGE_COUNT) {
        current_page = page;
    }
}

void hg_navigator_next(void) {
    current_page = (hg_page_id_t)((current_page + 1) % HG_PAGE_COUNT);
}

void hg_navigator_prev(void) {
    current_page = (hg_page_id_t)((current_page + HG_PAGE_COUNT - 1) % HG_PAGE_COUNT);
}

hg_page_id_t hg_navigator_current(void) {
    return current_page;
}
