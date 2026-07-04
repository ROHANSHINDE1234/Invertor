/*
 * gpio_driver.h — GPIO Driver Interface
 *
 * Generic, reusable GPIO functions. No pin numbers or port names appear here;
 * those live exclusively in hw_config.h. Callers pass port and pin as arguments.
 *
 * Functions:
 *   gpio_output_init    — plain push-pull digital output (CPU drives pin via ODR)
 *   gpio_af_output_init — alternate-function push-pull output (peripheral drives pin)
 *   gpio_write          — atomic set/clear using BSRR (safe inside ISR)
 *   gpio_toggle         — XOR-toggle via ODR
 */

#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include "stm32f1xx.h"
#include <stdint.h>

/*
 * gpio_output_init — Configure a pin as general-purpose push-pull output.
 *   CRx field = 0x1  (MODE=01 → 10 MHz, CNF=00 → GP push-pull)
 *   Use for: LED, PB7 (Q2, software-driven GPIO)
 */
void gpio_output_init(GPIO_TypeDef *port, uint8_t pin);

/*
 * gpio_af_output_init — Configure a pin as alternate-function push-pull output.
 *   CRx field = 0xB  (MODE=11 → 50 MHz, CNF=10 → AF push-pull)
 *   Use for: PB6 (Q1, TIM4 CH1 drives this pin directly via hardware PWM)
 *   Must NOT be used for pins that the CPU drives; peripheral owns the output level.
 */
void gpio_af_output_init(GPIO_TypeDef *port, uint8_t pin);

/*
 * gpio_write — Atomically set a pin HIGH (state=1) or LOW (state=0).
 *   Uses BSRR register: single-instruction, cannot be interrupted mid-operation.
 *   Safe to call from inside interrupt handlers (unlike ODR read-modify-write).
 */
void gpio_write(GPIO_TypeDef *port, uint8_t pin, uint8_t state);

/*
 * gpio_toggle — Flip a pin's current output state using XOR on ODR.
 *   Not interrupt-safe (read-modify-write). Use only in main-loop context.
 */
void gpio_toggle(GPIO_TypeDef *port, uint8_t pin);

#endif /* GPIO_DRIVER_H */
