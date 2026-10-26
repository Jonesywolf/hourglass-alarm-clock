#include <Arduino.h>
#include <initializer_list>
#include <unity.h>

#include "hg/config.h"
#include "hg/services/alarm.h"
#include "hg/services/sound.h"
#include "hg/services/store.h"

void test_alarm_choice_record_and_saves(void) {
    uint8_t bytes[] = {0x32, 0x41, 0x47, 0x48, 1, 6, 42, 80, 0, 0, 0, 0};
    hg_alarm_snapshot_t value = {};
    for (unsigned choice = 0; choice < HG_ALARM_SOUND_COUNT; ++choice) {
        bytes[8] = choice;
        TEST_ASSERT_TRUE(hg_store_test_decode_alarm(bytes, sizeof(bytes), &value));
        TEST_ASSERT_EQUAL(choice, value.sound);
        TEST_ASSERT_EQUAL(80, value.volume_pct);
        TEST_ASSERT_EQUAL(6, value.hour);
        TEST_ASSERT_EQUAL(42, value.minute);
        TEST_ASSERT_TRUE(value.enabled);
    }
    bytes[8] = 255;
    TEST_ASSERT_TRUE(hg_store_test_decode_alarm(bytes, sizeof(bytes), &value));
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_MALLETS, value.sound);
    bytes[9] = 1;
    TEST_ASSERT_FALSE(hg_store_test_decode_alarm(bytes, sizeof(bytes), &value));
    bytes[9] = 0;
    bytes[5] = 24;
    TEST_ASSERT_FALSE(hg_store_test_decode_alarm(bytes, sizeof(bytes), &value));
    bytes[5] = 6;
    TEST_ASSERT_FALSE(hg_store_test_decode_alarm(bytes, sizeof(bytes) - 1, &value));
    uint8_t legacy[] = {0x31, 0x41, 0x47, 0x48, 1, 6, 42, 0};
    TEST_ASSERT_TRUE(hg_store_test_decode_alarm(legacy, sizeof(legacy), &value));
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_MALLETS, value.sound);
    TEST_ASSERT_EQUAL(HG_ALARM_VOLUME_DEFAULT_PCT, value.volume_pct);

    hg_store_test_writer(0, false);
    hg_store_set_flash_allowed(false);
    hg_alarm_set_enabled(false);
    hg_alarm_commit_edit(6, 42, HG_ALARM_SOUND_MALLETS);
    hg_alarm_set_enabled(false);
    hg_store_status_t before, after;
    hg_store_get_status(&before);
    hg_alarm_commit_edit(6, 42, HG_ALARM_SOUND_WOODEN);
    hg_alarm_commit_edit(6, 42, HG_ALARM_SOUND_PLUCKED);
    hg_store_get_status(&after);
    TEST_ASSERT_EQUAL_UINT32(before.requested_generation, after.requested_generation);
    delay(HG_ALARM_STORE_DEBOUNCE_MS + 10);
    hg_alarm_update();
    hg_store_get_status(&after);
    TEST_ASSERT_EQUAL_UINT32(before.requested_generation + 1, after.requested_generation);
    hg_store_test_get_desired(&value);
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_PLUCKED, value.sound);
    hg_store_request_save_alarm(&value);
    hg_store_status_t same;
    hg_store_get_status(&same);
    TEST_ASSERT_EQUAL_UINT32(after.requested_generation, same.requested_generation);
    hg_alarm_set_enabled(true);
    hg_time_snapshot_t due = {true, 6, 42, 0};
    hg_alarm_test_check_time(&due);
    hg_alarm_snooze();
    hg_alarm_adjust_volume(1);
    delay(HG_ALARM_STORE_DEBOUNCE_MS + 10);
    hg_alarm_update();
    hg_store_test_get_desired(&value);
    TEST_ASSERT_EQUAL(6, value.hour);
    TEST_ASSERT_EQUAL(42, value.minute);
    TEST_ASSERT_EQUAL(HG_ALARM_SOUND_PLUCKED, value.sound);
    TEST_ASSERT_FALSE(value.snoozed);
    hg_alarm_set_enabled(false);
    hg_alarm_commit_edit(6, 42, HG_ALARM_SOUND_MALLETS);
}

void test_alarm_choice_pcm_and_scaling(void) {
    const hg_sound_id_t ids[] = {HG_SOUND_ALARM, HG_SOUND_ALARM_WOODEN, HG_SOUND_ALARM_PLUCKED};
    const uint32_t hashes[] = {0xd02a38bb, 0xf6128bb0, 0xe58f7998};
    hg_effects_snapshot_t effects = {};
    effects.phase = HG_EFFECT_RINGING;
    for (unsigned choice = 0; choice < HG_ALARM_SOUND_COUNT; ++choice) {
        hg_sound_id_t id = ids[choice];
        TEST_ASSERT_EQUAL(id, hg_sound_for_effects(&effects, (hg_alarm_sound_t)choice));
        TEST_ASSERT_TRUE(hg_sound_is_alarm(id));
        hg_sound_player_t player;
        TEST_ASSERT_TRUE(hg_sound_player_start(&player, id, HG_AUDIO_SAMPLE_RATE));
        TEST_ASSERT_EQUAL_UINT32(96000, player.frames);
        TEST_ASSERT_TRUE(player.loop);
        uint32_t hash = 2166136261U;
        for (uint32_t i = 0; i < player.frames; ++i) {
            hash = (hash ^ (uint16_t)player.pcm[i]) * 16777619U;
        }
        TEST_ASSERT_EQUAL_HEX32(hashes[choice], hash);
        TEST_ASSERT_FALSE(hg_sound_player_start(&player, id, HG_AUDIO_SAMPLE_RATE / 2));
        TEST_ASSERT_EQUAL_FLOAT(0, hg_sound_gain_at(id, 0, HG_AUDIO_SAMPLE_RATE));
        for (uint8_t pct : {5, 75, 100}) {
            hg_sound_player_t full, scaled;
            TEST_ASSERT_TRUE(hg_sound_player_start(&full, id, HG_AUDIO_SAMPLE_RATE, 100));
            TEST_ASSERT_TRUE(hg_sound_player_start(&scaled, id, HG_AUDIO_SAMPLE_RATE, pct));
            full.elapsed = scaled.elapsed = HG_ALARM_RAMP_MS * (HG_AUDIO_SAMPLE_RATE / 1000U);
            full.position = scaled.position = full.frames - 19;
            int16_t reference[137], out[137];
            TEST_ASSERT_EQUAL_UINT32(137, hg_sound_player_render(&full, reference, 137));
            for (unsigned offset = 0; offset < 137; ) {
                unsigned count = offset < 100 ? 17 : 1;
                if (count > 137 - offset) count = 137 - offset;
                TEST_ASSERT_EQUAL_UINT32(count, hg_sound_player_render(&scaled, out + offset, count));
                offset += count;
            }
            for (unsigned i = 0; i < 137; ++i) {
                TEST_ASSERT_INT_WITHIN(1, (int)(reference[i] * pct / 100.0f), out[i]);
                TEST_ASSERT_TRUE(abs((int)out[i]) <=
                    (int)(32768.0f * HG_ALARM_VOLUME_MAX_PCT / 100.0f * pct / 100.0f) + 1);
            }
            TEST_ASSERT_EQUAL_UINT32(118, full.position);
            TEST_ASSERT_EQUAL_UINT32(full.position, scaled.position);
            TEST_ASSERT_EQUAL_UINT32(full.elapsed, scaled.elapsed);
        }
    }
    TEST_ASSERT_FALSE(hg_sound_is_alarm(HG_SOUND_ALARM_ENTER));
    TEST_ASSERT_FALSE(hg_sound_is_alarm(HG_SOUND_NONE));
}
