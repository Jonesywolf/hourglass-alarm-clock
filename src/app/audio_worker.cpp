#include <Arduino.h>

#include "hg/config.h"
#include "hg/hal/speaker.h"
#include "hg/log.h"
#include "hg/services/sound.h"
#include "runtime_internal.h"

static int16_t chunk[HG_AUDIO_CHUNK_FRAMES];

static bool start_playback(hg_sound_player_t *player, hg_sound_id_t sound) {
    return hg_sound_player_start(player, sound, hg_speaker_sample_rate()) && hg_speaker_start();
}

/* Owns the speaker after init. Acts only on the latest UI command, stops immediately when it
 * changes, and always publishes the generation it applied so a stop is acknowledged. */
void hg_audio_worker_task(void *) {
    hg_runtime_wait_start();
    hg_sound_player_t player = {};
    hg_audio_command_t desired = {};
    hg_audio_status_t status = {};
    bool playing = false;
    bool retry_pending = false;
    uint32_t retry_at_ms = 0;
    for (;;) {
        hg_audio_command_t latest;
        if (xQueuePeek(hg_audio_command_mailbox, &latest, 0) == pdTRUE &&
            (latest.generation != desired.generation || latest.sound != desired.sound)) {
            desired = latest;
            if (playing) {
                hg_speaker_stop();
                playing = false;
            }
            retry_pending = false;
            if (hg_sound_available(desired.sound)) {
                playing = start_playback(&player, desired.sound);
                if (!playing) {
                    retry_pending = true;
                    retry_at_ms = millis() + HG_AUDIO_RETRY_MS;
                    HG_ERROR("audio", "Speaker start failed; retry scheduled\n");
                }
            }
            status.generation = desired.generation;
        }

        if (playing) {
            size_t frames = hg_sound_player_render(&player, chunk, HG_AUDIO_CHUNK_FRAMES);
            if (frames == 0) {
                hg_speaker_stop();
                playing = false;
            } else if (hg_speaker_write(chunk, frames, HG_AUDIO_WRITE_TIMEOUT_MS) < frames) {
                hg_speaker_stop();
                playing = false;
                retry_pending = player.loop;
                retry_at_ms = millis() + HG_AUDIO_RETRY_MS;
                HG_ERROR("audio", "Speaker write failed; stopped\n");
            }
        } else if (retry_pending && (int32_t)(millis() - retry_at_ms) >= 0) {
            retry_pending = false;
            playing = start_playback(&player, desired.sound);
            if (!playing) {
                retry_pending = true;
                retry_at_ms = millis() + HG_AUDIO_RETRY_MS;
            }
        }

        hg_speaker_stats_t stats;
        hg_speaker_get_stats(&stats);
        status.active = playing;
        status.underruns = stats.underruns;
        status.write_errors = stats.write_errors + stats.short_writes;
        status.updated_ms = millis();
        xQueueOverwrite(hg_audio_status_mailbox, &status);
        hg_runtime_record_audio(status.underruns, status.write_errors);
        hg_runtime_record_worker(3, 0);
        if (!playing) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
