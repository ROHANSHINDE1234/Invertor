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
 *   2. pwm_pushpull_init() — start push-pull switching waveform (input stage)
 *   3. pwm_set_duty()      — lock the push-pull to the fixed operating duty
 *   4. hbridge_init()      — start 50 Hz H-bridge output unfolder
 *   5. Idle — both stages run autonomously in hardware/ISR from here
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
    /* Step 1: Switch system clock to external 8 MHz crystal.
     * Must be first — all timer timing depends on this clock. */
    clock_init_hse();

    /* Step 2: Start push-pull PWM on PB6 and PB7.
     * Both pins are hardware PWM (center-aligned TIM4 CH1/CH2) — no ISR in the
     * gate path. Comes up at the safe startup duty (PWM_DUTY_MIN_TICKS). */
    pwm_pushpull_init();

    /* Step 3: Lock the push-pull to its fixed operating ON-time.
     * One write; CCR1/CCR2 preload applies it at the next period boundary.
     * No control loop — the duty never changes after this. */
    pwm_set_duty(PWM_DUTY_FIXED_TICKS);

    /* Step 4: Start the 50 Hz H-bridge output unfolder on PA4–PA7.
     * Free-runs on TIM3 with a 1 ms dead-time at each polarity swap;
     * independent of the push-pull, no phase-lock needed. */
    hbridge_init();

    /* Step 5: Nothing left to do in software.
     * The push-pull runs entirely in TIM4 hardware and the H-bridge in its
     * TIM3 ISR — main() just idles. */
    while (1)
    {
        /* idle */
    }
}
