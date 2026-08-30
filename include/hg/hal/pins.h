#ifndef HG_HAL_PINS_H
#define HG_HAL_PINS_H

/* Assigned pins are in use. -1 means "not wired yet". */

#define HG_PIN_TFT_SCK    12
#define HG_PIN_TFT_MOSI   11
#define HG_PIN_TFT_CS     10
#define HG_PIN_TFT_DC      9
#define HG_PIN_TFT_RST    14
#define HG_PIN_TFT_BL     (-1)

#define HG_PIN_TSL2591_SDA (-1)
#define HG_PIN_TSL2591_SCL (-1)

#define HG_PIN_TOUCH      (-1)

#define HG_PIN_ENC_A      (-1)
#define HG_PIN_ENC_B      (-1)
#define HG_PIN_ENC_BTN    (-1)

/* MAX98357A-class I2S amp (board may be marked MAX93857A). */
#define HG_PIN_I2S_BCLK   (-1)
#define HG_PIN_I2S_LRCLK  (-1)
#define HG_PIN_I2S_DOUT   (-1)
#define HG_PIN_I2S_SDMODE (-1)

#define HG_PIN_LED_STRIP  (-1)

#define HG_PIN_SEN66_SDA  (-1)
#define HG_PIN_SEN66_SCL  (-1)

#define HG_PIN_RTC_SDA    (-1)
#define HG_PIN_RTC_SCL    (-1)

#endif
