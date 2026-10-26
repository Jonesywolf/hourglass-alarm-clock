#include <Arduino.h>

#include "hg/config.h"
#include "hg/hal/speaker.h"
#include "hg/log.h"
#include "hg/services/sound.h"
#include "runtime_internal.h"

#ifdef HG_RUNTIME_TEST
#include <atomic>
static std::atomic<bool> fail_start(false);
static std::atomic<bool> fail_write(false);

void hg_runtime_test_audio_failure(bool start, bool write) {
    fail_start.store(start);
    fail_write.store(write);
}
#endif

static int16_t chunk[HG_AUDIO_CHUNK_FRAMES];

static bool start_playback(hg_sound_player_t *player, const hg_audio_command_t &command) {
    if (!hg_sound_player_start(player, command.sound, hg_speaker_sample_rate(),
                               command.alarm_volume_pct)) {
        return false;
    }
#ifdef HG_RUNTIME_TEST
    if (fail_start.exchange(false)) {
        return false;
    }
#endif
    return hg_speaker_start();
}

static bool write_chunk(void) {
#ifdef HG_RUNTIME_TEST
    if (fail_write.exchange(false)) {
        return false;
    }
#endif
    return hg_speaker_write(chunk, HG_AUDIO_CHUNK_FRAMES,
                            HG_AUDIO_WRITE_TIMEOUT_MS) == HG_AUDIO_CHUNK_FRAMES;
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
    uint32_t drain_frames = 0;
    for (;;) {
        hg_audio_command_t latest;
        if (xQueuePeek(hg_audio_command_mailbox, &latest, 0) == pdTRUE &&
            (latest.generation != desired.generation || latest.sound != desired.sound ||
             latest.alarm_volume_pct != desired.alarm_volume_pct)) {
            desired = latest;
            if (playing) {
                hg_speaker_stop();
                playing = false;
            }
            retry_pending = false;
            drain_frames = 0;
#ifdef HG_RUNTIME_TEST
            status.rendered_frames = status.drain_frames = status.start_attempts = 0;
#endif
            if (hg_sound_available(desired.sound)) {
#ifdef HG_RUNTIME_TEST
                ++status.start_attempts;
#endif
                playing = start_playback(&player, desired);
                if (!playing) {
                    retry_pending = player.loop;
                    retry_at_ms = millis() + HG_AUDIO_RETRY_MS;
                    HG_ERROR("audio", "Speaker start failed; looping sounds retry\n");
                }
            }
            status.generation = desired.generation;
        }

        if (playing) {
            size_t frames = hg_sound_player_render(&player, chunk, HG_AUDIO_CHUNK_FRAMES);
            bool draining = frames == 0;
            if (draining && drain_frames >= HG_AUDIO_DRAIN_FRAMES) {
                hg_speaker_stop();
                playing = false;
            } else {
                /* Flush the final samples through DMA before muting, checking commands
                 * between chunks so replacement and alarm preemption remain prompt. */
                for (size_t i = frames; i < HG_AUDIO_CHUNK_FRAMES; ++i) {
                    chunk[i] = 0;
                }
                if (!write_chunk()) {
                    hg_speaker_stop();
                    playing = false;
                    retry_pending = player.loop;
                    retry_at_ms = millis() + HG_AUDIO_RETRY_MS;
                    HG_ERROR("audio", "Speaker write failed; stopped\n");
                } else if (draining) {
                    drain_frames += HG_AUDIO_CHUNK_FRAMES;
#ifdef HG_RUNTIME_TEST
                    status.drain_frames = drain_frames;
#endif
                } else {
#ifdef HG_RUNTIME_TEST
                    status.rendered_frames += (uint32_t)frames;
#endif
                }
            }
        } else if (retry_pending && (int32_t)(millis() - retry_at_ms) >= 0) {
            retry_pending = false;
#ifdef HG_RUNTIME_TEST
            ++status.start_attempts;
#endif
            playing = start_playback(&player, desired);
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
