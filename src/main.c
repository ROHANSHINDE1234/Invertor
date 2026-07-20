/*
 * main.c — Application Entry Point
 *
 * HOD RULE: This file contains ONLY application-level logic.
 *   - No register writes (RCC->, TIM->, GPIO->, ADC->)
 *   - No hardware pin numbers or peripheral names
 *   - No timing constants (those live in hw_config.h)
 *   - Only driver function calls and application logic
 *
 * Sequence:
 *   1. clock_init_hse()    — switch to accurate external crystal
 *   2. adc_init()          — configure pot ADC on PA0
 *   3. pwm_pushpull_init() — start push-pull switching waveform
 *   4. Loop: read pot → map to duty → update PWM
 */

#include "hw_config.h"
#include "clock_config.h"
#include "adc_driver.h"
#include "gpio_driver.h"
#include "pwm_driver.h"

/*
 * map_adc_to_duty — Linear mapping from ADC reading to timer ticks.
 *
 * Input:  adc_val in [0 .. 4095]  (0V to 3.3V at pot wiper)
 * Output: duty_ticks in [PWM_DUTY_MIN_TICKS .. PWM_DUTY_MAX_TICKS]
 *                     = [16 .. 128] ticks = [2 µs .. 16 µs]
 *
 * Formula: duty = MIN + (adc_val × (MAX - MIN)) / 4095
 *
 * The result is also clamped again inside pwm_set_duty() as a second
 * safety layer, but the calculation here should already be in range.
 */
static uint16_t map_adc_to_duty(uint16_t adc_val)
{
    uint32_t range = (uint32_t)(PWM_DUTY_MAX_TICKS - PWM_DUTY_MIN_TICKS);
    return (uint16_t)(PWM_DUTY_MIN_TICKS + ((uint32_t)adc_val * range) / 4095u);
}

int main(void)
{
    /* Step 1: Switch system clock to external 8 MHz crystal.
     * Must be first — all timer and ADC timing depends on this clock. */
    clock_init_hse();

    /* Step 2: Initialize ADC for potentiometer reading.
     * Configures PA0, sets up continuous conversion, runs calibration. */
    adc_init();

    /* Step 3: Start push-pull PWM on PB6 and PB7.
     * Timer begins switching at startup duty. Both pins are hardware PWM
     * (center-aligned TIM4 CH1/CH2) — no ISR involved in gate driving.
     * Switching continues in the background from this point onward. */
    pwm_pushpull_init();

    /* Step 4: Control loop.
     * Reads pot position, maps to duty cycle, updates both PWM channels.
     * The PWM timer runs continuously in hardware — this loop only
     * adjusts the compare register values. */
    while (1)
    {
        uint16_t adc_val   = adc_read();
        uint16_t duty      = map_adc_to_duty(adc_val);
        pwm_set_duty(duty);

        /* Wait approximately one full switching period (40 µs) before
         * the next ADC read and duty update.
         *
         * Why: CCR1/CCR2 both have preload enabled, so an update takes
         * effect at the next period boundary regardless. Waiting here
         * just prevents the CPU from updating duty faster than the timer
         * can consume the new values.
         *
         * 800 iterations at 8 MHz ≈ 100 µs = 2.5 switching periods. */
        for (volatile uint32_t i = 0u; i < 800u; i++);
    }
}
