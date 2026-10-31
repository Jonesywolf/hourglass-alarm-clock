#ifndef HG_APP_SIMULATOR_H
#define HG_APP_SIMULATOR_H

#include <stdint.h>
#include "hg/types.h"

/* Browser target only; production firmware does not link these adapters. */
uint32_t hg_sim_now(void);
void hg_sim_advance(uint32_t ms);
bool hg_sim_input(int kind, int32_t value);
void hg_sim_cancel_inputs(void);
void hg_sim_capture_state(void);
void hg_sim_set_time(int hour, int minute, int second, bool valid);
void hg_sim_set_environment(int wifi, int status, float lux, float outdoor_c,
                            float indoor_c, float humidity, int aqi);
void hg_sim_seed_alarm(const hg_alarm_snapshot_t *alarm);
void hg_sim_set_fault(bool fault);
void hg_sim_render(void);
uint32_t hg_sim_frames(void);
uint8_t hg_sim_brightness(void);

#endif
