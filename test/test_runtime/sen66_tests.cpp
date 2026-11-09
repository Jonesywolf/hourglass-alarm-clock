#include <Arduino.h>
#include <SensirionCrc.h>
#include <SensirionErrors.h>
#include <math.h>
#include <unity.h>

#include "hg/config.h"
#include "hg/hal/sen66.h"
#include "hg/services/air_rating.h"
#include "hg/services/indoor_air.h"

static const uint16_t start_command = 0x0021;
static const uint16_t status_command = 0xD206;
static const uint16_t ready_command = 0x0202;
static const uint16_t measurement_command = 0x0300;

struct fake_sensor_t {
    uint32_t now_ms;
    uint32_t command_ms;
    uint32_t last_read_ms;
    uint32_t flags;
    uint16_t command;
    uint16_t words[9];
    uint16_t fail_send;
    uint16_t fail_receive;
    uint16_t receive_error;
    unsigned send_count;
    unsigned start_count;
    unsigned measurement_reads;
    unsigned timing_errors;
    bool ready;
    bool auto_ready;
};

static fake_sensor_t fake;
static bool fake_active;

static void put_word(uint8_t *payload, unsigned index, uint16_t value) {
    payload[index * 2U] = (uint8_t)(value >> 8);
    payload[index * 2U + 1U] = (uint8_t)value;
}

static uint16_t fake_send(uint16_t command) {
    fake.command = command;
    fake.command_ms = fake.now_ms;
    ++fake.send_count;
    if (command == start_command) {
        ++fake.start_count;
    }
    if (fake.fail_send == command) {
        fake.fail_send = 0;
        return (uint16_t)WriteError | I2cAddressNack;
    }
    return 0;
}

static uint16_t fake_receive(uint8_t *payload, uint8_t words) {
    if (fake.now_ms - fake.command_ms < 20U) {
        ++fake.timing_errors;
    }
    if (fake.fail_receive == fake.command) {
        fake.fail_receive = 0;
        return fake.receive_error;
    }
    if (fake.command == status_command && words == 2) {
        put_word(payload, 0, (uint16_t)(fake.flags >> 16));
        put_word(payload, 1, (uint16_t)fake.flags);
        return 0;
    }
    if (fake.command == ready_command && words == 1) {
        if (fake.auto_ready && fake.now_ms - fake.last_read_ms >= 1000U) {
            fake.ready = true;
        }
        put_word(payload, 0, fake.ready ? 1 : 0);
        return 0;
    }
    if (fake.command == measurement_command && words == 9) {
        for (unsigned i = 0; i < 9; ++i) {
            put_word(payload, i, fake.words[i]);
        }
        ++fake.measurement_reads;
        fake.ready = false;
        fake.last_read_ms = fake.now_ms;
        return 0;
    }
    return (uint16_t)ReadError | WrongNumberBytesError;
}

static void fake_begin(uint32_t now_ms = 0) {
    fake = {};
    fake.now_ms = now_ms;
    fake.last_read_ms = now_ms - 1000U;
    fake.words[0] = 123;
    fake.words[1] = 45;
    fake.words[2] = 67;
    fake.words[3] = 89;
    fake.words[4] = 5432;
    fake.words[5] = (uint16_t)(int16_t)-2150;
    fake.words[6] = 1200;
    fake.words[7] = 800;
    fake.words[8] = 890;
    fake.ready = true;
    fake.auto_ready = true;
    fake.receive_error = (uint16_t)ReadError | CRCError;
    hg_sen66_test_transport(fake_send, fake_receive);
    hg_sen66_init();
    fake_active = true;
}

void sen66_test_cleanup(void) {
    if (fake_active) {
        hg_sen66_test_transport(nullptr, nullptr);
        fake_active = false;
    }
}

static hg_sample_status_t tick(hg_sen66_sample_t *sample) {
    hg_sample_status_t result = hg_sen66_update(fake.now_ms, sample);
    fake.now_ms += 20U;
    return result;
}

static hg_sample_status_t wait_for(hg_sample_status_t wanted, hg_sen66_sample_t *sample,
                                   uint32_t timeout_ms) {
    uint32_t start = fake.now_ms;
    do {
        hg_sample_status_t result = tick(sample);
        if (result == wanted) {
            return result;
        }
    } while (fake.now_ms - start <= timeout_ms);
    return HG_SAMPLE_WAITING;
}

static hg_indoor_snapshot_t service_tick(void) {
    hg_indoor_air_update(fake.now_ms);
    fake.now_ms += 20U;
    hg_indoor_snapshot_t snapshot;
    hg_indoor_air_get_snapshot(&snapshot);
    return snapshot;
}

static hg_indoor_snapshot_t wait_service(hg_sample_status_t status, uint32_t timeout_ms) {
    uint32_t start = fake.now_ms;
    hg_indoor_snapshot_t snapshot = {};
    do {
        snapshot = service_tick();
        if (snapshot.status == status) {
            return snapshot;
        }
    } while (fake.now_ms - start <= timeout_ms);
    return snapshot;
}

void test_sen66_scaling_warmup_and_cadence(void) {
    fake_begin();
    fake.words[7] = INT16_MAX;
    fake.words[8] = UINT16_MAX;
    hg_sen66_sample_t sample = {};
    TEST_ASSERT_EQUAL(HG_SAMPLE_WAITING, tick(&sample));
    for (unsigned i = 0; i < 50; ++i) {
        TEST_ASSERT_EQUAL(HG_SAMPLE_WAITING, tick(&sample));
    }
    TEST_ASSERT_EQUAL_UINT(1, fake.send_count);
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, wait_for(HG_SAMPLE_READY, &sample, 3000U));
    uint32_t first = fake.now_ms;
    TEST_ASSERT_TRUE(sample.valid);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.3f, sample.pm1_0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.5f, sample.pm2_5);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 54.32f, sample.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.75f, sample.temp_c);
    TEST_ASSERT_EQUAL_UINT16(120, sample.voc_index);
    TEST_ASSERT_EQUAL_UINT16(0, sample.nox_index);
    TEST_ASSERT_EQUAL_UINT16(0, sample.co2_ppm);
    fake.words[1] = UINT16_MAX;
    fake.words[7] = 800;
    fake.words[8] = 890;
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, wait_for(HG_SAMPLE_READY, &sample, 3000U));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(HG_INDOOR_SAMPLE_MS, fake.now_ms - first);
    TEST_ASSERT_TRUE(isnan(sample.pm2_5));
    TEST_ASSERT_EQUAL_UINT16(80, sample.nox_index);
    TEST_ASSERT_EQUAL_UINT16(890, sample.co2_ppm);
    for (unsigned i = 0; i < 4; ++i) {
        fake.words[i] = UINT16_MAX;
    }
    for (unsigned i = 4; i < 8; ++i) {
        fake.words[i] = INT16_MAX;
    }
    fake.words[8] = UINT16_MAX;
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, wait_for(HG_SAMPLE_READY, &sample, 3000U));
    TEST_ASSERT_FALSE(sample.valid);
    TEST_ASSERT_TRUE(isnan(sample.pm1_0));
    TEST_ASSERT_TRUE(isnan(sample.pm2_5));
    TEST_ASSERT_TRUE(isnan(sample.pm4_0));
    TEST_ASSERT_TRUE(isnan(sample.pm10));
    TEST_ASSERT_TRUE(isnan(sample.humidity));
    TEST_ASSERT_TRUE(isnan(sample.temp_c));
    TEST_ASSERT_EQUAL_UINT16(0, sample.voc_index);
    TEST_ASSERT_EQUAL_UINT16(0, sample.nox_index);
    TEST_ASSERT_EQUAL_UINT16(0, sample.co2_ppm);
    TEST_ASSERT_EQUAL_UINT(0, fake.timing_errors);
}

void test_sen66_crc_retry_and_device_faults(void) {
    const uint8_t vector[] = {0xBE, 0xEF};
    TEST_ASSERT_EQUAL_HEX8(0x92, generateCRC(vector, sizeof(vector), CRC31_ff));
    fake_begin();
    fake.fail_send = start_command;
    hg_sen66_sample_t sample = {};
    TEST_ASSERT_EQUAL(HG_SAMPLE_UNAVAILABLE, tick(&sample));
    for (unsigned i = 0; i < HG_SENSOR_RETRY_MS / 20U - 1U; ++i) {
        TEST_ASSERT_EQUAL(HG_SAMPLE_UNAVAILABLE, tick(&sample));
    }
    TEST_ASSERT_EQUAL_UINT(1, fake.start_count);
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, wait_for(HG_SAMPLE_READY, &sample, 8000U));
    fake.fail_receive = status_command;
    fake.receive_error = (uint16_t)ReadError | CRCError;
    TEST_ASSERT_EQUAL(HG_SAMPLE_ERROR, wait_for(HG_SAMPLE_ERROR, &sample, 3000U));
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, wait_for(HG_SAMPLE_READY, &sample, 8000U));
    fake.fail_receive = ready_command;
    fake.receive_error = (uint16_t)ReadError | NotEnoughDataError;
    TEST_ASSERT_EQUAL(HG_SAMPLE_ERROR, wait_for(HG_SAMPLE_ERROR, &sample, 3000U));
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, wait_for(HG_SAMPLE_READY, &sample, 8000U));
    fake.flags = 1UL << 4;
    TEST_ASSERT_EQUAL(HG_SAMPLE_ERROR, wait_for(HG_SAMPLE_ERROR, &sample, 3000U));
    fake.flags = 0;
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, wait_for(HG_SAMPLE_READY, &sample, 8000U));
    fake.words[6] = 5001;
    TEST_ASSERT_EQUAL(HG_SAMPLE_ERROR, wait_for(HG_SAMPLE_ERROR, &sample, 3000U));
}

void test_sen66_no_data_and_clock_rollover(void) {
    fake_begin(UINT32_MAX - 500U);
    fake.ready = false;
    fake.auto_ready = false;
    hg_sen66_sample_t sample = {};
    TEST_ASSERT_EQUAL(HG_SAMPLE_ERROR, wait_for(HG_SAMPLE_ERROR, &sample, 8000U));
    TEST_ASSERT_EQUAL_UINT(0, fake.measurement_reads);
    fake.ready = true;
    fake.auto_ready = true;
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, wait_for(HG_SAMPLE_READY, &sample, 8000U));
    TEST_ASSERT_TRUE(sample.valid);
    TEST_ASSERT_EQUAL_UINT(0, fake.timing_errors);
}

void test_sen66_pause_discards_pending_sample(void) {
    fake_begin();
    hg_sen66_sample_t sample = {};
    TEST_ASSERT_EQUAL(HG_SAMPLE_WAITING, tick(&sample));
    hg_sen66_set_paused(true, fake.now_ms);
    hg_sen66_set_paused(false, fake.now_ms);
    for (unsigned i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL(HG_SAMPLE_WAITING, tick(&sample));
    }
    TEST_ASSERT_EQUAL_UINT(1, fake.send_count);
    TEST_ASSERT_EQUAL(HG_SAMPLE_WAITING, tick(&sample));
    TEST_ASSERT_EQUAL_HEX16(measurement_command, fake.command);

    fake_begin();
    uint32_t start = fake.now_ms;
    while (fake.command != measurement_command && fake.now_ms - start < 3000U) {
        TEST_ASSERT_NOT_EQUAL(HG_SAMPLE_READY, tick(&sample));
    }
    TEST_ASSERT_EQUAL_HEX16(measurement_command, fake.command);
    hg_sen66_set_paused(true, fake.now_ms);
    unsigned sends = fake.send_count;
    for (unsigned i = 0; i < 75; ++i) {
        TEST_ASSERT_EQUAL(HG_SAMPLE_PAUSED, tick(&sample));
    }
    TEST_ASSERT_EQUAL_UINT(sends, fake.send_count);
    fake.words[8] = 1200;
    hg_sen66_set_paused(false, fake.now_ms);
    start = fake.now_ms;
    while (fake.measurement_reads == 0 && fake.now_ms - start < 2000U) {
        TEST_ASSERT_NOT_EQUAL(HG_SAMPLE_READY, tick(&sample));
    }
    TEST_ASSERT_EQUAL_UINT(1, fake.measurement_reads);
    fake.words[8] = 1500;
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, wait_for(HG_SAMPLE_READY, &sample, 3000U));
    TEST_ASSERT_EQUAL_UINT16(1500, sample.co2_ppm);
}

void test_indoor_air_retains_and_replaces_samples(void) {
    fake_begin();
    hg_indoor_air_init();
    hg_indoor_snapshot_t good = wait_service(HG_SAMPLE_READY, 3000U);
    TEST_ASSERT_TRUE(good.valid);
    TEST_ASSERT_EQUAL(HG_AIR_GOOD, good.level);
    fake.fail_receive = status_command;
    hg_indoor_snapshot_t failed = wait_service(HG_SAMPLE_ERROR, 3000U);
    TEST_ASSERT_EQUAL(HG_SAMPLE_ERROR, failed.status);
    TEST_ASSERT_TRUE(failed.valid);
    TEST_ASSERT_EQUAL_UINT32(good.sampled_ms, failed.sampled_ms);
    TEST_ASSERT_EQUAL_UINT16(good.co2_ppm, failed.co2_ppm);
    fake.words[8] = UINT16_MAX;
    fake.words[1] = 90;
    hg_indoor_snapshot_t partial = wait_service(HG_SAMPLE_READY, 8000U);
    TEST_ASSERT_EQUAL(HG_SAMPLE_READY, partial.status);
    TEST_ASSERT_NOT_EQUAL(good.sampled_ms, partial.sampled_ms);
    TEST_ASSERT_EQUAL_UINT16(0, partial.co2_ppm);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 9.0f, partial.pm2_5);
    TEST_ASSERT_EQUAL(HG_AIR_UNKNOWN, partial.level);
    for (unsigned i = 0; i < 4; ++i) {
        fake.words[i] = UINT16_MAX;
    }
    for (unsigned i = 4; i < 8; ++i) {
        fake.words[i] = INT16_MAX;
    }
    hg_indoor_snapshot_t stale = wait_service(HG_SAMPLE_STALE, 3000U);
    TEST_ASSERT_EQUAL(HG_SAMPLE_STALE, stale.status);
    TEST_ASSERT_EQUAL_UINT32(partial.sampled_ms, stale.sampled_ms);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, partial.pm2_5, stale.pm2_5);
    hg_indoor_air_set_paused(true, fake.now_ms);
    hg_indoor_air_get_snapshot(&partial);
    TEST_ASSERT_EQUAL(HG_SAMPLE_PAUSED, partial.status);
    hg_indoor_air_set_paused(false, fake.now_ms);
    hg_indoor_air_get_snapshot(&partial);
    TEST_ASSERT_EQUAL(HG_SAMPLE_STALE, partial.status);
}

void test_indoor_rating_partial_and_thresholds(void) {
    for (unsigned mask = 0; mask < 8; ++mask) {
        hg_indoor_snapshot_t indoor = {};
        indoor.valid = true;
        indoor.co2_ppm = (mask & 1U) ? 800 : 0;
        indoor.pm2_5 = (mask & 2U) ? 4.0f : NAN;
        indoor.voc_index = (mask & 4U) ? 100 : 0;
        hg_air_rate_indoor(&indoor);
        TEST_ASSERT_EQUAL(mask == 7 ? HG_AIR_GOOD : HG_AIR_UNKNOWN, indoor.level);
        TEST_ASSERT_EQUAL(HG_AIR_CAUSE_NONE, indoor.cause);
    }
    for (unsigned cause = 0; cause < 3; ++cause) {
        hg_indoor_snapshot_t indoor = {};
        indoor.valid = true;
        indoor.pm2_5 = NAN;
        if (cause == 0) indoor.co2_ppm = HG_INDOOR_CO2_POOR_PPM;
        if (cause == 1) indoor.pm2_5 = HG_INDOOR_PM25_POOR;
        if (cause == 2) indoor.voc_index = HG_INDOOR_VOC_POOR;
        hg_air_rate_indoor(&indoor);
        TEST_ASSERT_EQUAL(HG_AIR_POOR, indoor.level);
        TEST_ASSERT_EQUAL((int)HG_AIR_CAUSE_CO2 + (int)cause, indoor.cause);
    }
    hg_indoor_snapshot_t indoor = {};
    indoor.valid = true;
    indoor.co2_ppm = HG_INDOOR_CO2_FAIR_PPM;
    indoor.pm2_5 = HG_INDOOR_PM25_FAIR;
    indoor.voc_index = HG_INDOOR_VOC_FAIR;
    hg_air_rate_indoor(&indoor);
    TEST_ASSERT_EQUAL(HG_AIR_FAIR, indoor.level);
    TEST_ASSERT_EQUAL(HG_AIR_CAUSE_CO2, indoor.cause);
    indoor.co2_ppm = HG_INDOOR_CO2_FAIR_PPM - 1;
    indoor.pm2_5 = HG_INDOOR_PM25_FAIR - 0.1f;
    indoor.voc_index = HG_INDOOR_VOC_FAIR - 1;
    hg_air_rate_indoor(&indoor);
    TEST_ASSERT_EQUAL(HG_AIR_GOOD, indoor.level);
    indoor.valid = false;
    hg_air_rate_indoor(&indoor);
    TEST_ASSERT_EQUAL(HG_AIR_UNKNOWN, indoor.level);
}
