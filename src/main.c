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
 *   1. hbridge_safe_state() — PA1 enable LOW + PA4–PA7 gates OFF (GPIO only)
 *   2. pwm_safe_state()     — PB6/PB7 gates OFF (GPIO only; Stage-1 SD tied low)
 *   3. clock_init_hse()     — switch to accurate external crystal
 *   4. hbridge_init()       — bootstrap precharge, then start 50 Hz H-bridge unfolder
 *   5. pwm_pushpull_init()  — start push-pull switching waveform (input stage)
 *   6. pwm_soft_start()     — ramp duty to PWM_DUTY_FIXED_TICKS over ~0.5 s
 *   7. Idle — both stages run autonomously in hardware/ISR from here
 *
 * The potentiometer/ADC duty control has been removed: the push-pull now runs
 * at the fixed PWM_DUTY_FIXED_TICKS operating point set in hw_config.h. To put
 * the pot back, restore adc_init()/adc_read() and the mapping loop (see git
 * history) — adc_driver.c is still in the tree.
 */

#include "hw_config.h"
#include "clock_config.h"
#include "gpio_driver.h"
#include "pwm_driver.h"
#include "hbridge_driver.h"

int main(void)
{
    /* Step 1: Force the safe state as GPIO, BEFORE the clock switch, so nothing
     * is driven on at power-up: H-bridge enable (PA1) LOW and all four bridge
     * gates OFF, then the push-pull gates (PB6/PB7) OFF. The Stage-1 IR2110 SD
     * is tied low (always enabled), so PB6/PB7 must read OFF from the first instant. */
    hbridge_safe_state();
    pwm_safe_state();

    /* Step 2: Switch system clock to external 8 MHz crystal.
     * Must precede the timers — all timer timing depends on this clock. */
    clock_init_hse();

    /* Step 3: Start the 50 Hz H-bridge unfolder on PA4–PA7. Precharges the
     * IR2110 bootstraps (LA+LB on, 5 ms), enables the bridge, then free-runs
     * on TIM3 with a 1 ms dead-time at each polarity swap. */
    hbridge_init();

    /* Step 4: Start push-pull PWM on PB6/PB7 (center-aligned TIM4 CH1/CH2,
     * no ISR). Comes up at the safe startup duty (PWM_DUTY_MIN_TICKS). */
    pwm_pushpull_init();

    /* Step 5: Soft-start — ramp the push-pull duty from minimum to the fixed
     * operating ON-time over ~0.5 s, instead of a hard step. */
    pwm_soft_start(PWM_DUTY_FIXED_TICKS);

    /* Step 6: Nothing left to do in software — both stages run autonomously. */
    while (1)
    {
        /* idle */
    }
}
