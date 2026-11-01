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
void hg_sim_set_environment(int wifi, float lux, int rssi);
/* Synthetic forecast: hourly temps follow low..high (peak 3pm). Wet conditions rain for the next
 * 4 hours; dry ones with rain_pct >= the rain threshold get rain starting 3 hours from now. */
typedef struct {
    int status;     /* hg_sample_status_t; UNAVAILABLE reports a failed fetch with no data. */
    int condition;  /* hg_weather_condition_t */
    float temp_c, feels_c, high_c, low_c;
    float wind_kph, gust_kph, uv;
    int rain_pct;
    int aqi;
    int air_status;
} hg_sim_weather_t;
void hg_sim_set_weather(const hg_sim_weather_t *weather);
void hg_sim_set_indoor(int status, float temp_c, float humidity, int co2_ppm, float pm2_5,
                       int voc_index);
/* Read once by app setup, like the NVS setting. */
void hg_sim_set_units(int units);
void hg_sim_seed_alarm(const hg_alarm_snapshot_t *alarm);
void hg_sim_set_fault(bool fault);
void hg_sim_render(void);
uint32_t hg_sim_frames(void);
uint8_t hg_sim_brightness(void);

#endif
