#ifndef HG_HAL_DISPLAY_BUS_INTERNAL_H
#define HG_HAL_DISPLAY_BUS_INTERNAL_H

#include <Arduino_DataBus.h>
#include <driver/spi_master.h>

class hg_display_bus_t : public Arduino_DataBus {
public:
    bool begin(int32_t speed = GFX_NOT_DEFINED, int8_t mode = SPI_MODE0) override;
    void beginWrite(void) override;
    void endWrite(void) override;
    void writeCommand(uint8_t value) override;
    void writeCommand16(uint16_t value) override;
    void write(uint8_t value) override;
    void write16(uint16_t value) override;
    void writeRepeat(uint16_t value, uint32_t count) override;
    void writePixels(uint16_t *data, uint32_t count) override;
    void writeBytes(uint8_t *data, uint32_t count) override;
    void queue_pixels(const uint8_t *data, uint32_t count);
    bool finish(bool wait, uint32_t *completed_us);

private:
    static void transfer_done(spi_transaction_t *transaction);
    void transfer(const uint8_t *data, uint32_t count, bool command);
    spi_device_handle_t device = nullptr;
    spi_transaction_t transaction = {};
    volatile uint32_t completed = 0;
};

#endif
