#include "hg/services/sound.h"

#include <math.h>

#include "hg/config.h"
#include "sound_internal.h"

typedef struct {
    uint16_t hz;
    uint16_t duration_ms;
    bool tap;
} tone_t;

static const tone_t enter[] = {{440, 80, true}};
static const tone_t field[] = {{620, 40, true}};
static const tone_t save[] = {{523, 70, false}, {0, 12, false}, {659, 100, false}};
static const tone_t enable[] = {{659, 140, false}};
static const tone_t disable[] = {{392, 140, false}};

typedef struct {
    const int16_t *pcm;
    const uint32_t *frames;
    bool loop;
    float start_gain;
    float end_gain;
    uint32_t ramp_ms;
    uint32_t fade_in_ms;
    const tone_t *tones;
    uint8_t tone_count;
} sound_descriptor_t;

static const sound_descriptor_t sounds[HG_SOUND_COUNT] = {
    /* HG_SOUND_NONE */ {nullptr, nullptr, false, 0.0f, 0.0f, 0, 0},
    /* HG_SOUND_ALARM */ {hg_alarm_clip_pcm, &hg_alarm_clip_frames, true,
                          HG_ALARM_VOLUME_START_PCT / 100.0f, HG_ALARM_VOLUME_MAX_PCT / 100.0f,
                          HG_ALARM_RAMP_MS, HG_AUDIO_FADE_IN_MS},
    /* HG_SOUND_UI_NAVIGATE */ {nullptr, nullptr, false, 0.0f, 0.0f, 0, 0},
    /* HG_SOUND_UI_ALARM_EDIT */ {nullptr, nullptr, false, 0.0f, 0.0f, 0, 0},
    /* HG_SOUND_ALARM_ENTER */ {nullptr, nullptr, false, 0, 0, 0, 0, enter, 1},
    /* HG_SOUND_ALARM_FIELD */ {nullptr, nullptr, false, 0, 0, 0, 0, field, 1},
    /* HG_SOUND_ALARM_SAVE */ {nullptr, nullptr, false, 0, 0, 0, 0, save, 3},
    /* HG_SOUND_ALARM_ENABLE */ {nullptr, nullptr, false, 0, 0, 0, 0, enable, 1},
    /* HG_SOUND_ALARM_DISABLE */ {nullptr, nullptr, false, 0, 0, 0, 0, disable, 1},
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
    case HG_FEEDBACK_ALARM_ENTER:
        return HG_SOUND_ALARM_ENTER;
    case HG_FEEDBACK_ALARM_FIELD:
        return HG_SOUND_ALARM_FIELD;
    case HG_FEEDBACK_ALARM_SAVE:
        return HG_SOUND_ALARM_SAVE;
    case HG_FEEDBACK_ALARM_ENABLE:
        return HG_SOUND_ALARM_ENABLE;
    case HG_FEEDBACK_ALARM_DISABLE:
        return HG_SOUND_ALARM_DISABLE;
    default:
        return HG_SOUND_NONE;
    }
}

bool hg_sound_available(hg_sound_id_t id) {
    const sound_descriptor_t *sound = descriptor(id);
    return sound->tones || (sound->pcm && sound->frames && *sound->frames > 0);
}

float hg_sound_gain_at(hg_sound_id_t id, uint32_t elapsed_frames, uint32_t sample_rate) {
    const sound_descriptor_t *sound = descriptor(id);
    if (sound->tones) {
        return HG_UI_CUE_VOLUME_PCT / 100.0f;
    }
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

bool hg_sound_player_start(hg_sound_player_t *player, hg_sound_id_t id, uint32_t sample_rate,
                           uint8_t alarm_volume_pct) {
    if (!player) {
        return false;
    }
    *player = {};
    if (!hg_sound_available(id) || sample_rate != HG_AUDIO_SAMPLE_RATE ||
        alarm_volume_pct < HG_ALARM_VOLUME_MIN_PCT || alarm_volume_pct > 100) {
        player->finished = true;
        return false;
    }
    const sound_descriptor_t *sound = descriptor(id);
    player->id = id;
    player->pcm = sound->pcm;
    if (sound->tones) {
        for (unsigned i = 0; i < sound->tone_count; ++i) {
            player->frames += sound->tones[i].duration_ms * sample_rate / 1000U;
        }
    } else {
        player->frames = *sound->frames;
    }
    player->loop = sound->loop;
    player->sample_rate = sample_rate;
    player->alarm_volume_pct = alarm_volume_pct;
    return true;
}

static int16_t tone_sample(const sound_descriptor_t *sound, uint32_t position,
                           uint32_t rate) {
    for (unsigned i = 0; i < sound->tone_count; ++i) {
        const tone_t &tone = sound->tones[i];
        uint32_t length = tone.duration_ms * rate / 1000U;
        if (position >= length) {
            position -= length;
            continue;
        }
        if (!tone.hz) {
            return 0;
        }
        float t = (float)position / rate;
        float duration = (float)length / rate;
        float phase = 6.28318530718f * tone.hz * t;
        float attack = t < 0.008f ? 0.5f - 0.5f * cosf(3.14159265359f * t / 0.008f) :
                                    1.0f;
        float decay = expf(-(tone.tap ? 5.0f : 3.0f) * t / duration);
        float wave = tone.tap ? 0.8f * sinf(phase) + 0.2f * sinf(phase * 1.47f) :
                                0.94f * sinf(phase) + 0.05f * sinf(phase * 2.0f) +
                                0.01f * sinf(phase * 3.0f);
        uint32_t edge = rate * 16U / 1000U;
        uint32_t remaining = length - 1U - position;
        float release = remaining < edge ?
                        0.5f - 0.5f * cosf(3.14159265359f * (float)remaining / edge) : 1.0f;
        return (int16_t)(wave * attack * decay * release * 32767.0f *
                         (HG_UI_CUE_VOLUME_PCT / 100.0f));
    }
    return 0;
}

size_t hg_sound_player_render(hg_sound_player_t *player, int16_t *out, size_t frames) {
    if (!player || !out || player->finished) {
        return 0;
    }
    const sound_descriptor_t *sound = descriptor(player->id);
    if (sound->tones) {
        size_t count = player->frames - player->position;
        if (count > frames) {
            count = frames;
        }
        for (size_t i = 0; i < count; ++i) {
            out[i] = tone_sample(sound, player->position++, player->sample_rate);
        }
        player->elapsed += (uint32_t)count;
        player->finished = player->position == player->frames;
        return count;
    }
    if (!player->pcm) {
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
    if (player->id == HG_SOUND_ALARM) {
        start *= player->alarm_volume_pct / 100.0f;
        end *= player->alarm_volume_pct / 100.0f;
    }
    for (size_t i = 0; i < produced; ++i) {
        float gain = start + (end - start) * ((float)i / (float)produced);
        float value = out[i] * gain;
        out[i] = (int16_t)(value > 32767.0f ? 32767.0f : value < -32768.0f ? -32768.0f : value);
    }
    player->elapsed += (uint32_t)produced;
    return produced;
}
