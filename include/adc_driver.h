#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include "stm32f1xx.h"

void     adc_init(void);
uint16_t adc_read(void);

#endif