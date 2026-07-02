#ifndef PWM_DRIVER_H
#define PWM_DRIVER_H

#include "stm32f1xx.h"

void pwm_pushpull_init(void);
void pwm_set_duty(uint16_t duty_ticks);

#endif