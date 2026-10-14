#include "display_bus_internal.h"

#include <driver/gpio.h>
#include <esp_timer.h>

#include "hg/config.h"
#include "hg/hal/pins.h"

void IRAM_ATTR hg_display_bus_t::transfer_done(spi_transaction_t *transaction) {
    if (transaction->user) {
        static_cast<hg_display_bus_t *>(transaction->user)->completed =
            (uint32_t)esp_timer_get_time();
    }
}

bool hg_display_bus_t::begin(int32_t speed, int8_t mode) {
    ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)HG_PIN_TFT_DC, GPIO_MODE_OUTPUT));
    spi_bus_config_t bus = {};
    bus.mosi_io_num = HG_PIN_TFT_MOSI;
    bus.miso_io_num = -1;
    bus.sclk_io_num = HG_PIN_TFT_SCK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = HG_DISP_HOR_RES * HG_DISP_DRAW_BUF_LINES * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t config = {};
    config.mode = mode == GFX_NOT_DEFINED ? SPI_MODE0 : mode;
    config.clock_speed_hz = speed == GFX_NOT_DEFINED ? 40000000 : speed;
    config.spics_io_num = HG_PIN_TFT_CS;
    config.queue_size = 1;
    config.flags = SPI_DEVICE_NO_DUMMY;
    config.post_cb = transfer_done;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &config, &device));
    return true;
}

void hg_display_bus_t::beginWrite(void) {
    // IDF 4.4 requires portMAX_DELAY here; SPI2 has no other device/owner.
    ESP_ERROR_CHECK(spi_device_acquire_bus(device, portMAX_DELAY));
}

void hg_display_bus_t::endWrite(void) {
    spi_device_release_bus(device);
}

void hg_display_bus_t::transfer(const uint8_t *data, uint32_t count, bool command) {
    gpio_set_level((gpio_num_t)HG_PIN_TFT_DC, command ? 0 : 1);
    spi_transaction_t transfer = {};
    transfer.length = count * 8U;
    if (count <= sizeof(transfer.tx_data)) {
        transfer.flags = SPI_TRANS_USE_TXDATA;
        memcpy(transfer.tx_data, data, count);
    } else {
        transfer.tx_buffer = data;
    }
    ESP_ERROR_CHECK(spi_device_polling_transmit(device, &transfer));
}

void hg_display_bus_t::writeCommand(uint8_t value) {
    transfer(&value, 1, true);
}

void hg_display_bus_t::writeCommand16(uint16_t value) {
    uint8_t bytes[] = {(uint8_t)(value >> 8), (uint8_t)value};
    transfer(bytes, sizeof(bytes), true);
}

void hg_display_bus_t::write(uint8_t value) {
    transfer(&value, 1, false);
}

void hg_display_bus_t::write16(uint16_t value) {
    uint8_t bytes[] = {(uint8_t)(value >> 8), (uint8_t)value};
    transfer(bytes, sizeof(bytes), false);
}

void hg_display_bus_t::writeRepeat(uint16_t value, uint32_t count) {
    uint16_t pixels[128];
    for (uint16_t &pixel : pixels) {
        pixel = __builtin_bswap16(value);
    }
    while (count) {
        uint32_t chunk = count > 128 ? 128 : count;
        transfer((const uint8_t *)pixels, chunk * sizeof(uint16_t), false);
        count -= chunk;
    }
}

void hg_display_bus_t::writePixels(uint16_t *data, uint32_t count) {
    uint16_t pixels[128];
    while (count) {
        uint32_t chunk = count > 128 ? 128 : count;
        for (unsigned i = 0; i < chunk; ++i) {
            pixels[i] = __builtin_bswap16(data[i]);
        }
        transfer((const uint8_t *)pixels, chunk * sizeof(uint16_t), false);
        data += chunk;
        count -= chunk;
    }
}

void hg_display_bus_t::writeBytes(uint8_t *data, uint32_t count) {
    while (count) {
        uint32_t chunk = count > 2048 ? 2048 : count;
        transfer(data, chunk, false);
        data += chunk;
        count -= chunk;
    }
}

void hg_display_bus_t::queue_pixels(const uint8_t *data, uint32_t count) {
    configASSERT(((uintptr_t)data & 3U) == 0 && count % 4U == 0);
    gpio_set_level((gpio_num_t)HG_PIN_TFT_DC, 1);
    transaction = {};
    transaction.tx_buffer = data;
    transaction.length = count * 8U;
    transaction.user = this;
    ESP_ERROR_CHECK(spi_device_queue_trans(device, &transaction, pdMS_TO_TICKS(20)));
}

bool hg_display_bus_t::finish(bool wait, uint32_t *completed_us) {
    spi_transaction_t *result = nullptr;
    esp_err_t error = spi_device_get_trans_result(device, &result, wait ? pdMS_TO_TICKS(20) : 0);
    if (!wait && error == ESP_ERR_TIMEOUT) {
        return false;
    }
    ESP_ERROR_CHECK(error);
    configASSERT(result == &transaction);
    *completed_us = completed;
    return true;
}
