/*
 * clock_config.h — System Clock Configuration Interface
 *
 * Switches the system clock from the default internal HSI oscillator
 * (±1% accuracy, temperature-sensitive) to the external crystal HSE
 * (~20–50 ppm, stable with temperature).
 *
 * This matters for PWM timing: HSE keeps the switching frequency at
 * exactly 25 kHz even as the board warms up during operation.
 */

#ifndef CLOCK_CONFIG_H
#define CLOCK_CONFIG_H

#include "stm32f1xx.h"

/*
 * clock_init_hse — Switch system clock to external 8 MHz crystal (HSE).
 *
 * MUST be called as the very first line of main(), before any peripheral
 * initialization. All timer and ADC clock derivations assume 8 MHz HSE.
 *
 * If the external crystal is not populated or not oscillating, this function
 * hangs indefinitely waiting for HSERDY — an intentional hardware diagnostic.
 */

void clock_init_hse(void);

#endif /* CLOCK_CONFIG_H */