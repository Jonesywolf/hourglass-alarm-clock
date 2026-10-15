#ifndef HG_HAL_SPEAKER_H
#define HG_HAL_SPEAKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t write_errors;
    uint32_t short_writes;
    /* Inferred: the producer returned to the DMA ring after it could have drained. */
    uint32_t underruns;
} hg_speaker_stats_t;

/* Installs I2S in the stopped, amp-muted state. False if unwired or the driver fails. */
bool hg_speaker_init(void);
bool hg_speaker_ready(void);
uint32_t hg_speaker_sample_rate(void);
/* Starts clocks and unmutes the amp. Call from the audio worker only. */
bool hg_speaker_start(void);
/* Mono 16-bit frames; blocks up to timeout_ms for DMA space. Returns frames accepted. */
size_t hg_speaker_write(const int16_t *mono, size_t frames, uint32_t timeout_ms);
/* Mutes the amp immediately, discards queued audio and stops clocks. */
void hg_speaker_stop(void);
void hg_speaker_get_stats(hg_speaker_stats_t *out);

#endif
