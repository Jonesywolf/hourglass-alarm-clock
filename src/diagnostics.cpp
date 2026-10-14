#include "hg/diagnostics.h"

void hg_timing_record(hg_timing_stats_t *stats, uint32_t elapsed_us) {
    ++stats->samples;
    if (elapsed_us > stats->max_us) {
        stats->max_us = elapsed_us;
    }
    uint32_t bucket = elapsed_us / 1000U + (elapsed_us % 1000U != 0);
    if (bucket > 65) {
        bucket = 65;
    }
    ++stats->milliseconds[bucket];
}

uint32_t hg_timing_percentile_us(const hg_timing_stats_t *stats, uint32_t percentile) {
    if (stats->samples == 0 || percentile == 0 || percentile > 100) {
        return 0;
    }
    uint32_t rank = (uint32_t)(((uint64_t)stats->samples * percentile + 99U) / 100U);
    uint32_t count = 0;
    for (uint32_t i = 0; i < 66; ++i) {
        count += stats->milliseconds[i];
        if (count >= rank) {
            return i == 65 ? stats->max_us : i * 1000U;
        }
    }
    return stats->max_us;
}
