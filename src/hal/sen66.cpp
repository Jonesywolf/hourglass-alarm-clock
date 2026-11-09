#include "hg/hal/sen66.h"

#include <math.h>
#include <Wire.h>
#include <SensirionI2CCommunication.h>
#include <SensirionI2CRxFrame.h>
#include <SensirionI2CTxFrame.h>

#ifdef HG_RUNTIME_TEST
#include <atomic>
#endif

#include "hg/config.h"
#include "hg/log.h"

static const uint8_t address = 0x6B;
static const uint16_t start_measurement = 0x0021;
static const uint16_t read_status = 0xD206;
static const uint16_t data_ready = 0x0202;
static const uint16_t read_measurement = 0x0300;
static const uint32_t first_measurement_ms = 1100U;
static const uint32_t response_delay_ms = 20U;
static const uint32_t resume_settle_ms = 50U;
static const uint32_t status_faults = (1UL << 4) | (1UL << 6) | (1UL << 7) |
                                      (1UL << 9) | (1UL << 10) | (1UL << 11) |
                                      (1UL << 12) | (1UL << 21);

enum class phase_t {
    start,
    first_measurement,
    status_command,
    status_response,
    ready_command,
    ready_response,
    measurement_command,
    measurement_response,
    interval,
    discard_command,
    discard_response,
    retry,
    paused
};

static phase_t phase;
static uint32_t phase_started_ms;
static uint32_t cycle_started_ms;
static uint32_t last_progress_ms;
static bool started;
static bool seen_device;
static hg_sample_status_t failure_status;

#ifdef HG_RUNTIME_TEST
static std::atomic<bool> forced_error(false);

void hg_sen66_test_force_error(bool enabled) {
    forced_error.store(enabled);
}
#endif

static uint16_t send_wire(uint16_t command) {
#ifdef HG_RUNTIME_TEST
    if (forced_error.load()) {
        return UINT16_MAX;
    }
#endif
    uint8_t buffer[2];
    SensirionI2CTxFrame frame =
        SensirionI2CTxFrame::createWithUInt16Command(command, buffer, sizeof(buffer));
    return SensirionI2CCommunication::sendFrame(address, frame, Wire);
}

static uint16_t receive_wire(uint8_t *payload, uint8_t words) {
#ifdef HG_RUNTIME_TEST
    if (forced_error.load()) {
        return UINT16_MAX;
    }
#endif
    SensirionI2CRxFrame frame(payload, (size_t)words * 2U);
    return SensirionI2CCommunication::receiveFrame(address, (size_t)words * 3U, frame, Wire);
}

static uint16_t (*send_command)(uint16_t) = send_wire;
static uint16_t (*receive_words)(uint8_t *, uint8_t) = receive_wire;

#ifdef HG_RUNTIME_TEST
void hg_sen66_test_transport(hg_sen66_test_send_t send, hg_sen66_test_receive_t receive) {
    send_command = send ? send : send_wire;
    receive_words = receive ? receive : receive_wire;
}
#endif

static hg_sample_status_t fail(uint32_t now_ms, const char *operation, uint32_t detail) {
    HG_ERROR("sen66", "%s failed (0x%08lx); retry scheduled\n",
             operation, (unsigned long)detail);
    failure_status = seen_device ? HG_SAMPLE_ERROR : HG_SAMPLE_UNAVAILABLE;
    started = false;
    phase = phase_t::retry;
    phase_started_ms = now_ms;
    return failure_status;
}

static uint16_t word(const uint8_t *payload, unsigned index) {
    return (uint16_t)(((uint16_t)payload[index * 2U] << 8) | payload[index * 2U + 1U]);
}

static float unsigned_scaled(uint16_t raw, float divisor) {
    return raw == UINT16_MAX ? NAN : (float)raw / divisor;
}

static float signed_scaled(uint16_t raw, float divisor) {
    if (raw == INT16_MAX) {
        return NAN;
    }
    int32_t value = raw < 0x8000U ? (int32_t)raw : (int32_t)raw - 0x10000L;
    return (float)value / divisor;
}

static bool index_value(uint16_t raw, uint16_t *out) {
    if (raw == INT16_MAX) {
        *out = 0;
        return true;
    }
    if (raw < 10U || raw > 5000U) {
        return false;
    }
    *out = (uint16_t)((raw + 5U) / 10U);
    return true;
}

static bool decode(const uint8_t *payload, hg_sen66_sample_t *out) {
    hg_sen66_sample_t sample = {};
    sample.pm1_0 = unsigned_scaled(word(payload, 0), 10.0f);
    sample.pm2_5 = unsigned_scaled(word(payload, 1), 10.0f);
    sample.pm4_0 = unsigned_scaled(word(payload, 2), 10.0f);
    sample.pm10 = unsigned_scaled(word(payload, 3), 10.0f);
    sample.humidity = signed_scaled(word(payload, 4), 100.0f);
    sample.temp_c = signed_scaled(word(payload, 5), 200.0f);
    if (!index_value(word(payload, 6), &sample.voc_index) ||
        !index_value(word(payload, 7), &sample.nox_index)) {
        return false;
    }
    uint16_t co2 = word(payload, 8);
    sample.co2_ppm = co2 == UINT16_MAX ? 0 : co2;
    sample.valid = !isnan(sample.pm1_0) || !isnan(sample.pm2_5) ||
                   !isnan(sample.pm4_0) || !isnan(sample.pm10) ||
                   !isnan(sample.humidity) || !isnan(sample.temp_c) ||
                   sample.voc_index != 0 || sample.nox_index != 0 || sample.co2_ppm != 0;
    *out = sample;
    return true;
}

void hg_sen66_init(void) {
    phase = phase_t::start;
    phase_started_ms = 0;
    cycle_started_ms = 0;
    last_progress_ms = 0;
    started = false;
    seen_device = false;
    failure_status = HG_SAMPLE_UNAVAILABLE;
}

void hg_sen66_set_paused(bool paused, uint32_t now_ms) {
    if (paused) {
        phase = phase_t::paused;
    } else if (phase == phase_t::paused) {
        phase = started ? phase_t::discard_command : phase_t::start;
        phase_started_ms = now_ms;
        last_progress_ms = now_ms;
    }
}

hg_sample_status_t hg_sen66_update(uint32_t now_ms, hg_sen66_sample_t *out) {
    if (!out) {
        HG_ERROR("sen66", "Missing sample output\n");
        return HG_SAMPLE_ERROR;
    }

    uint16_t error;
    uint8_t payload[18];
    switch (phase) {
        case phase_t::start:
            error = send_command(start_measurement);
            if (error) {
                return fail(now_ms, "Start measurement", error);
            }
            seen_device = true;
            started = true;
            phase_started_ms = now_ms;
            last_progress_ms = now_ms;
            phase = phase_t::first_measurement;
            break;
        case phase_t::first_measurement:
            if (now_ms - phase_started_ms >= first_measurement_ms) {
                phase = phase_t::status_command;
            }
            break;
        case phase_t::status_command:
            cycle_started_ms = now_ms;
            error = send_command(read_status);
            if (error) {
                return fail(now_ms, "Read device status", error);
            }
            phase_started_ms = now_ms;
            phase = phase_t::status_response;
            break;
        case phase_t::status_response:
            if (now_ms - phase_started_ms < response_delay_ms) {
                break;
            }
            error = receive_words(payload, 2);
            if (error) {
                return fail(now_ms, "Device status response", error);
            }
            {
                uint32_t flags = ((uint32_t)word(payload, 0) << 16) | word(payload, 1);
                if (flags & status_faults) {
                    return fail(now_ms, "Device status", flags);
                }
            }
            phase = phase_t::ready_command;
            break;
        case phase_t::ready_command:
            error = send_command(data_ready);
            if (error) {
                return fail(now_ms, "Check data ready", error);
            }
            phase_started_ms = now_ms;
            phase = phase_t::ready_response;
            break;
        case phase_t::ready_response:
            if (now_ms - phase_started_ms < response_delay_ms) {
                break;
            }
            error = receive_words(payload, 1);
            if (error) {
                return fail(now_ms, "Data-ready response", error);
            }
            if (word(payload, 0) > 1U) {
                return fail(now_ms, "Invalid data-ready response", word(payload, 0));
            }
            if (word(payload, 0) == 0U) {
                if (now_ms - last_progress_ms >= HG_SENSOR_RETRY_MS) {
                    return fail(now_ms, "No new measurement", 0);
                }
                phase = phase_t::interval;
            } else {
                phase = phase_t::measurement_command;
            }
            break;
        case phase_t::measurement_command:
        case phase_t::discard_command:
            /* A pause can interrupt the start command's 50 ms execution time. */
            if (phase == phase_t::discard_command &&
                now_ms - phase_started_ms < resume_settle_ms) {
                break;
            }
            error = send_command(read_measurement);
            if (error) {
                return fail(now_ms, "Read measurement", error);
            }
            phase_started_ms = now_ms;
            phase = phase == phase_t::discard_command ? phase_t::discard_response :
                                                       phase_t::measurement_response;
            break;
        case phase_t::measurement_response:
        case phase_t::discard_response:
            if (now_ms - phase_started_ms < response_delay_ms) {
                break;
            }
            error = receive_words(payload, 9);
            if (error) {
                return fail(now_ms, "Measurement response", error);
            }
            if (phase == phase_t::discard_response) {
                last_progress_ms = now_ms;
                cycle_started_ms = now_ms;
                phase = phase_t::interval;
                break;
            }
            if (!decode(payload, out)) {
                HG_ERROR("sen66", "Invalid VOC/NOx raw values: %u/%u\n",
                         (unsigned)word(payload, 6), (unsigned)word(payload, 7));
                return fail(now_ms, "Invalid measurement", 0);
            }
            last_progress_ms = now_ms;
            phase = phase_t::interval;
            return HG_SAMPLE_READY;
        case phase_t::interval:
            if (now_ms - cycle_started_ms >= HG_INDOOR_SAMPLE_MS) {
                phase = phase_t::status_command;
            }
            break;
        case phase_t::retry:
            if (now_ms - phase_started_ms >= HG_SENSOR_RETRY_MS) {
                phase = phase_t::start;
            }
            return failure_status;
        case phase_t::paused:
            return HG_SAMPLE_PAUSED;
    }
    return HG_SAMPLE_WAITING;
}