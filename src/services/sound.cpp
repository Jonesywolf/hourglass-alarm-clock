#include "hg/services/sound.h"

#include "hg/config.h"
#include "sound_internal.h"

typedef struct {
    const int16_t *pcm;
    const uint32_t *frames;
    bool loop;
    float start_gain;
    float end_gain;
    uint32_t ramp_ms;
    uint32_t fade_in_ms;
} sound_descriptor_t;

static const sound_descriptor_t sounds[HG_SOUND_COUNT] = {
    /* HG_SOUND_NONE */ {nullptr, nullptr, false, 0.0f, 0.0f, 0, 0},
    /* HG_SOUND_ALARM */ {hg_alarm_clip_pcm, &hg_alarm_clip_frames, true,
                          HG_ALARM_VOLUME_START_PCT / 100.0f, HG_ALARM_VOLUME_MAX_PCT / 100.0f,
                          HG_ALARM_RAMP_MS, HG_AUDIO_FADE_IN_MS},
    /* HG_SOUND_UI_NAVIGATE */ {nullptr, nullptr, false, 0.0f, 0.0f, 0, 0},
    /* HG_SOUND_UI_ALARM_EDIT */ {nullptr, nullptr, false, 0.0f, 0.0f, 0, 0},
};

static const sound_descriptor_t *descriptor(hg_sound_id_t id) {
    return (unsigned)id < HG_SOUND_COUNT ? &sounds[id] : &sounds[HG_SOUND_NONE];
}

hg_sound_id_t hg_sound_for_effects(const hg_effects_snapshot_t *effects) {
    if (!effects) {
        return HG_SOUND_NONE;
    }
    if (effects->phase == HG_EFFECT_RINGING) {
        return HG_SOUND_ALARM;
    }
    if (effects->phase != HG_EFFECT_NORMAL) {
        return HG_SOUND_NONE;
    }
    switch (effects->feedback) {
    case HG_FEEDBACK_NAVIGATE:
        return HG_SOUND_UI_NAVIGATE;
    case HG_FEEDBACK_ALARM_EDIT:
        return HG_SOUND_UI_ALARM_EDIT;
    default:
        return HG_SOUND_NONE;
    }
}

bool hg_sound_available(hg_sound_id_t id) {
    const sound_descriptor_t *sound = descriptor(id);
    return sound->pcm && sound->frames && *sound->frames > 0;
}

float hg_sound_gain_at(hg_sound_id_t id, uint32_t elapsed_frames, uint32_t sample_rate) {
    const sound_descriptor_t *sound = descriptor(id);
    float gain = sound->end_gain;
    uint32_t ramp_frames = (uint32_t)((uint64_t)sound->ramp_ms * sample_rate / 1000U);
    if (ramp_frames > 0 && elapsed_frames < ramp_frames) {
        float t = (float)elapsed_frames / (float)ramp_frames;
        gain = sound->start_gain + (sound->end_gain - sound->start_gain) * t;
    }
    uint32_t fade_frames = (uint32_t)((uint64_t)sound->fade_in_ms * sample_rate / 1000U);
    if (fade_frames > 0 && elapsed_frames < fade_frames) {
        gain *= (float)elapsed_frames / (float)fade_frames;
    }
    return gain;
}

bool hg_sound_player_start(hg_sound_player_t *player, hg_sound_id_t id, uint32_t sample_rate) {
    if (!player) {
        return false;
    }
    *player = {};
    if (!hg_sound_available(id) || sample_rate != HG_AUDIO_SAMPLE_RATE) {
        player->finished = true;
        return false;
    }
    const sound_descriptor_t *sound = descriptor(id);
    player->id = id;
    player->pcm = sound->pcm;
    player->frames = *sound->frames;
    player->loop = sound->loop;
    player->sample_rate = sample_rate;
    return true;
}

size_t hg_sound_player_render(hg_sound_player_t *player, int16_t *out, size_t frames) {
    if (!player || !out || player->finished || !player->pcm) {
        return 0;
    }
    size_t produced = 0;
    while (produced < frames) {
        if (player->position >= player->frames) {
            if (!player->loop) {
                player->finished = true;
                break;
            }
            player->position = 0;
        }
        size_t run = player->frames - player->position;
        if (run > frames - produced) {
            run = frames - produced;
        }
        for (size_t i = 0; i < run; ++i) {
            out[produced + i] = player->pcm[player->position + i];
        }
        player->position += (uint32_t)run;
        produced += run;
    }
    if (produced == 0) {
        return 0;
    }
    float start = hg_sound_gain_at(player->id, player->elapsed, player->sample_rate);
    float end = hg_sound_gain_at(player->id, player->elapsed + (uint32_t)produced,
                                 player->sample_rate);
    for (size_t i = 0; i < produced; ++i) {
        float gain = start + (end - start) * ((float)i / (float)produced);
        float value = out[i] * gain;
        out[i] = (int16_t)(value > 32767.0f ? 32767.0f : value < -32768.0f ? -32768.0f : value);
    }
    player->elapsed += (uint32_t)produced;
    return produced;
}
