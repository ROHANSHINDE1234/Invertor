/*
 * adc_driver.h — ADC Driver Interface
 *
 * Reads the potentiometer voltage on PA0 (ADC1, Channel 0).
 * Returns a 12-bit value proportional to the pot position.
 *
 * Result mapping:
 *   0    → pot fully CCW (0V at PA0)
 *   4095 → pot fully CW  (3.3V at PA0)
 */

#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include "stm32f1xx.h"
#include <stdint.h>

void     adc_init(void);
uint16_t adc_read(void);

#endif /* ADC_DRIVER_H */