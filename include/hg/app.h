#ifndef HG_APP_H
#define HG_APP_H

void hg_app_setup(void);
void hg_app_loop(void);

#ifdef HG_RUNTIME_TEST
void hg_app_test_hide_boot_overlay(bool hide);
#endif

#endif
