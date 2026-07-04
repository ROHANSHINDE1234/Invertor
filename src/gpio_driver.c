/*
 * gpio_driver.c — GPIO Driver Implementation
 *
 * All STM32F103 GPIO register operations are here.
 * No pin numbers or port names are hardcoded — all come from hw_config.h
 * via the caller's arguments.
 */

#include "gpio_driver.h"

/*
 * gpio_clock_enable — Private helper: enable APB2 clock for a GPIO port.
 *
 * Called by both init functions so callers never need to manually enable
 * the clock. Marked static so it is invisible outside this file.
 *
 * On STM32F103, all GPIO ports are on APB2. Each port has a dedicated
 * enable bit in RCC->APB2ENR.
 */
static void gpio_clock_enable(GPIO_TypeDef *port)
{
    if      (port == GPIOA) { RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; }
    else if (port == GPIOB) { RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; }
    else if (port == GPIOC) { RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; }
    else if (port == GPIOD) { RCC->APB2ENR |= RCC_APB2ENR_IOPDEN; }
}

/*
 * gpio_output_init — General-purpose push-pull output.
 *
 * CRx 4-bit field encoding:
 *   Bits [1:0] = MODE = 01 → output, max 10 MHz
 *   Bits [3:2] = CNF  = 00 → general-purpose push-pull
 *   Combined: 0x1
 *
 * Pins 0–7  use CRL; pins 8–15 use CRH.
 * Each pin occupies a 4-bit field at offset (4 × pin_within_register).
 */
void gpio_output_init(GPIO_TypeDef *port, uint8_t pin)
{
    gpio_clock_enable(port);

    if (pin < 8u) {
        port->CRL &= ~(0xFu << (4u * pin));
        port->CRL |=  (0x1u << (4u * pin));
    } else {
        port->CRH &= ~(0xFu << (4u * (pin - 8u)));
        port->CRH |=  (0x1u << (4u * (pin - 8u)));
    }
}

/*
 * gpio_af_output_init — Alternate-function push-pull output.
 *
 * CRx 4-bit field encoding:
 *   Bits [1:0] = MODE = 11 → output, max 50 MHz  (fast edges for PWM)
 *   Bits [3:2] = CNF  = 10 → alternate-function push-pull
 *   Combined: 0xB
 *
 * Required when a timer peripheral owns the output level instead of the CPU.
 * Without this setting, TIM4's PWM output on PB6 would be ignored — the pin
 * would stay at whatever ODR says, completely independent of the timer.
 */
void gpio_af_output_init(GPIO_TypeDef *port, uint8_t pin)
{
    gpio_clock_enable(port);

    if (pin < 8u) {
        port->CRL &= ~(0xFu << (4u * pin));
        port->CRL |=  (0xBu << (4u * pin));
    } else {
        port->CRH &= ~(0xFu << (4u * (pin - 8u)));
        port->CRH |=  (0xBu << (4u * (pin - 8u)));
    }
}

/*
 * gpio_write — Atomically set or clear a pin using BSRR.
 *
 * BSRR (Bit Set/Reset Register) is write-only and operates in one instruction:
 *   Lower 16 bits: writing a 1 to bit N sets pin N HIGH
 *   Upper 16 bits: writing a 1 to bit N+16 clears pin N LOW
 *
 * Unlike ODR |= / ODR &= ~, BSRR is not a read-modify-write operation.
 * It cannot be interrupted between the read and the write — making it
 * safe to call from inside interrupt handlers without disabling interrupts.
 */
void gpio_write(GPIO_TypeDef *port, uint8_t pin, uint8_t state)
{
    if (state) {
        port->BSRR = (1u << pin);           /* SET:   pin HIGH */
    } else {
        port->BSRR = (1u << (pin + 16u));   /* RESET: pin LOW  */
    }
}

/*
 * gpio_toggle — Flip a pin's output state via XOR on ODR.
 *
 * Note: ODR ^= is a read-modify-write. Not interrupt-safe.
 * Use only in main-loop context where no ISR writes the same pin.
 */
void gpio_toggle(GPIO_TypeDef *port, uint8_t pin)
{
    port->ODR ^= (1u << pin);
}
