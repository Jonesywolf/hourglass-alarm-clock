#ifndef HG_SERVICES_SOUND_H
#define HG_SERVICES_SOUND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hg/services/effects.h"

/* Navigation and generic edit feedback remain silent; semantic alarm cues are synthesized. */
typedef enum {
    HG_SOUND_NONE = 0,
    HG_SOUND_ALARM,
    HG_SOUND_UI_NAVIGATE,
    HG_SOUND_UI_ALARM_EDIT,
    HG_SOUND_ALARM_ENTER,
    HG_SOUND_ALARM_FIELD,
    HG_SOUND_ALARM_SAVE,
    HG_SOUND_ALARM_ENABLE,
    HG_SOUND_ALARM_DISABLE,
    HG_SOUND_COUNT
} hg_sound_id_t;

/* Hardware-free PCM/tone renderer state: mono 16-bit at HG_AUDIO_SAMPLE_RATE. */
typedef struct {
    hg_sound_id_t id;
    const int16_t *pcm;
    uint32_t frames;
    uint32_t position;
    uint32_t elapsed;
    uint32_t sample_rate;
    bool loop;
    bool finished;
} hg_sound_player_t;

hg_sound_id_t hg_sound_for_effects(const hg_effects_snapshot_t *effects);
bool hg_sound_available(hg_sound_id_t id);
bool hg_sound_player_start(hg_sound_player_t *player, hg_sound_id_t id, uint32_t sample_rate);
/* Renders up to `frames` samples. Returns 0 once a non-looping sound has ended. */
size_t hg_sound_player_render(hg_sound_player_t *player, int16_t *out, size_t frames);
/* Alarm ramp times fade-in, or the cue gain cap (the tone envelope is sample-local). */
float hg_sound_gain_at(hg_sound_id_t id, uint32_t elapsed_frames, uint32_t sample_rate);

#endif
