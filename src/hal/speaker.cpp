#include "hg/hal/speaker.h"

#include <Arduino.h>
#include <driver/i2s.h>

#include "hg/config.h"
#include "hg/hal/pins.h"
#include "hg/log.h"

static const i2s_port_t port = I2S_NUM_0;
/* MAX98357A takes standard I2S; mono is duplicated into both slots so SD_MODE's
 * channel-select level does not matter. */
static int16_t stereo[HG_AUDIO_CHUNK_FRAMES * 2];
static bool ready = false;
static bool running = false;
static uint32_t last_write_end_us = 0;
static hg_speaker_stats_t stats;

static void set_amp(bool on) {
    digitalWrite(HG_PIN_I2S_SDMODE, on ? HIGH : LOW);
}

bool hg_speaker_init(void) {
    if (ready) {
        return true;
    }
    if (HG_PIN_I2S_BCLK < 0 || HG_PIN_I2S_LRCLK < 0 || HG_PIN_I2S_DOUT < 0) {
        return false;
    }
    if (HG_PIN_I2S_SDMODE >= 0) {
        pinMode(HG_PIN_I2S_SDMODE, OUTPUT);
        set_amp(false);
    }
    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = HG_AUDIO_SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = HG_AUDIO_DMA_BUF_COUNT;
    config.dma_buf_len = HG_AUDIO_CHUNK_FRAMES;
    config.tx_desc_auto_clear = true;
    if (i2s_driver_install(port, &config, 0, nullptr) != ESP_OK) {
        HG_ERROR("speaker", "I2S driver install failed\n");
        return false;
    }
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = HG_PIN_I2S_BCLK;
    pins.ws_io_num = HG_PIN_I2S_LRCLK;
    pins.data_out_num = HG_PIN_I2S_DOUT;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    if (i2s_set_pin(port, &pins) != ESP_OK) {
        HG_ERROR("speaker", "I2S pin setup failed\n");
        i2s_driver_uninstall(port);
        return false;
    }
    i2s_zero_dma_buffer(port);
    i2s_stop(port);
    stats = {};
    running = false;
    ready = true;
    return true;
}

bool hg_speaker_ready(void) {
    return ready;
}

uint32_t hg_speaker_sample_rate(void) {
    return HG_AUDIO_SAMPLE_RATE;
}

bool hg_speaker_start(void) {
    if (!ready) {
        return false;
    }
    if (running) {
        return true;
    }
    i2s_zero_dma_buffer(port);
    if (i2s_start(port) != ESP_OK) {
        return false;
    }
    running = true;
    last_write_end_us = micros();
    /* Let the amp see clocks and silence before it unmutes. */
    vTaskDelay(pdMS_TO_TICKS(2));
    if (HG_PIN_I2S_SDMODE >= 0) {
        set_amp(true);
    }
    return true;
}

size_t hg_speaker_write(const int16_t *mono, size_t frames, uint32_t timeout_ms) {
    if (!running || !mono) {
        return 0;
    }
    if (frames > HG_AUDIO_CHUNK_FRAMES) {
        frames = HG_AUDIO_CHUNK_FRAMES;
    }
    /* After a write returns the ring holds ~(count-1) chunks; a longer gap risks running dry. */
    const uint32_t drain_us = (HG_AUDIO_DMA_BUF_COUNT - 1U) * HG_AUDIO_CHUNK_FRAMES *
                              1000U / (HG_AUDIO_SAMPLE_RATE / 1000U);
    if (micros() - last_write_end_us > drain_us) {
        ++stats.underruns;
    }
    for (size_t i = 0; i < frames; ++i) {
        stereo[2 * i] = mono[i];
        stereo[2 * i + 1] = mono[i];
    }
    size_t bytes = frames * 2 * sizeof(int16_t);
    size_t written = 0;
    esp_err_t err = i2s_write(port, stereo, bytes, &written, pdMS_TO_TICKS(timeout_ms));
    last_write_end_us = micros();
    if (err != ESP_OK) {
        ++stats.write_errors;
    } else if (written < bytes) {
        ++stats.short_writes;
    }
    return written / (2 * sizeof(int16_t));
}

void hg_speaker_stop(void) {
    if (!ready) {
        return;
    }
    if (HG_PIN_I2S_SDMODE >= 0) {
        set_amp(false);
    }
    i2s_zero_dma_buffer(port);
    i2s_stop(port);
    running = false;
}

void hg_speaker_get_stats(hg_speaker_stats_t *out) {
    if (out) {
        *out = stats;
    }
}
