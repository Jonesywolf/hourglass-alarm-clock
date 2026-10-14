#ifndef HG_DIAGNOSTICS_H
#define HG_DIAGNOSTICS_H

#include <stdint.h>

typedef struct {
    uint32_t samples;
    uint32_t max_us;
    uint32_t milliseconds[66];
} hg_timing_stats_t;

void hg_timing_record(hg_timing_stats_t *stats, uint32_t elapsed_us);
uint32_t hg_timing_percentile_us(const hg_timing_stats_t *stats, uint32_t percentile);

#endif
