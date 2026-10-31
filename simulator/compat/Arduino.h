#ifndef HG_SIMULATOR_ARDUINO_H
#define HG_SIMULATOR_ARDUINO_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t millis(void);
uint32_t micros(void);
void delay(uint32_t ms);
#ifdef __cplusplus
}
#endif

#endif
