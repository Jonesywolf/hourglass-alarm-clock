#ifndef HG_SERVICES_SOUND_H
#define HG_SERVICES_SOUND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hg/services/effects.h"

/* UI_* ids are reserved for interaction feedback. They have no sample yet and render
 * as unavailable (silence); add a clip and a descriptor row in sound.cpp to enable one. */
typedef enum {
    HG_SOUND_NONE = 0,
    HG_SOUND_ALARM,
    HG_SOUND_UI_NAVIGATE,
    HG_SOUND_UI_ALARM_EDIT,
    HG_SOUND_COUNT
} hg_sound_id_t;

/* Hardware-free renderer state: mono 16-bit PCM at HG_AUDIO_SAMPLE_RATE. */
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
/* Linear gain (0..1) after `elapsed_frames` of playback: volume ramp times fade-in. */
float hg_sound_gain_at(hg_sound_id_t id, uint32_t elapsed_frames, uint32_t sample_rate);

#endif
