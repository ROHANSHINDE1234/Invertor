/*
 * hw_config.h — Hardware Configuration
 *
 * THE SINGLE SOURCE OF TRUTH for all hardware-specific definitions in this project.
 *
 * HOD RULE: No peripheral address, pin number, timing constant, or hardware
 * #define may appear in any other file. If hardware changes (pin moved, timer
 * swapped, clock speed changed), edit ONLY THIS FILE.
 *
 * Sections:
 *   1. System Clock
 *   2. Onboard LED
 *   3. Push-Pull PWM — Pin Assignments
 *   4. Push-Pull PWM — Timer Constants (derived arithmetic shown inline)
 *   5. Push-Pull PWM — Duty Cycle Limits
 *   6. Potentiometer ADC Input
 */

#ifndef HW_CONFIG_H
#define HW_CONFIG_H

/* ════════════════════════════════════════════════════════════════════
 * 1. SYSTEM CLOCK
 *    HSE = external crystal. Confirmed by reading SystemCoreClock
 *    in the debugger (returned 8,000,000). Used in all tick derivations.
 * ════════════════════════════════════════════════════════════════════ */
#define SYSCLK_HZ               8000000UL   /* 8 MHz HSE external crystal */

/* ════════════════════════════════════════════════════════════════════
 * 2. ONBOARD LED
 *    Blue Pill onboard LED is on Port C, pin 13.
 * ════════════════════════════════════════════════════════════════════ */
#define LED_PORT                GPIOC
#define LED_PIN                 13

/* ════════════════════════════════════════════════════════════════════
 * 3. PUSH-PULL PWM — PIN ASSIGNMENTS
 *
 *    TIM4 is on APB1 (not APB2 where GPIO and TIM1 live).
 *    PB6 = TIM4 CH1 → Q1: hardware PWM output, alternate-function mode
 *    PB7 = TIM4 CH2 → Q2: hardware PWM output, alternate-function mode
 *
 *    Both channels are hardware PWM (center-aligned mode) — there is
 *    no software/ISR-driven GPIO in the gate-drive path. See pwm_driver.c
 *    for why: an earlier revision drove PB7 from TIM4_IRQHandler, which
 *    raced the hardware-timed PB6 edge and could overlap under interrupt
 *    latency, worst at high duty.
 * ════════════════════════════════════════════════════════════════════ */
#define PWM_PORT                GPIOB
#define PWM_PIN_Q1              6           /* PB6: hardware PWM (TIM4 CH1)     */
#define PWM_PIN_Q2              7           /* PB7: hardware PWM (TIM4 CH2)     */

/* ════════════════════════════════════════════════════════════════════
 * 4. PUSH-PULL PWM — TIMER CONSTANTS
 *
 *    Switching frequency: 25 kHz  →  Period = 40 µs
 *    Timer prescaler (PSC): 0     →  1 tick = 1/8 MHz = 0.125 µs
 *
 *    TIM4 runs in CENTER-ALIGNED mode: CNT sweeps 0 → ARR → 0 once per
 *    switching period, so ARR is the HALF-period tick count, not the
 *    full period:
 *      ARR = 20 µs / 0.125 µs = 160   →  full period = 2×160 = 320 ticks = 40 µs
 *
 *    Waveform targets (Rev E specification), duty_ticks = desired ON-time:
 *      Q1 (PB6, PWM mode 1, HIGH while CNT<CCR1): pulse of width
 *        2×CCR1, centered on the trough (CNT=0 / period boundary).
 *      Q2 (PB7, PWM mode 2, HIGH while CNT>=CCR2): pulse of width
 *        2×(ARR-CCR2), centered on the peak (CNT=ARR) — exactly half
 *        a period away from Q1, by construction of the timer hardware.
 *      pwm_set_duty() sets CCR1 = CCR2 = duty_ticks/2 from center,
 *      i.e. CCR1 = duty_ticks/2, CCR2 = ARR - duty_ticks/2.
 *
 *    Startup safe values (immediately overwritten by ADC on first loop):
 *      CCR1_STARTUP = PWM_DUTY_MIN_TICKS / 2  (safest state, minimum duty)
 *      CCR2_STARTUP = PWM_HALF_PERIOD_TICKS - PWM_DUTY_MIN_TICKS / 2
 * ════════════════════════════════════════════════════════════════════ */
#define PWM_FREQ_HZ             25000UL
#define PWM_PSC                 0

#define PWM_HALF_PERIOD_TICKS   160         /* 20 µs — ARR value in center-aligned mode */

/* ════════════════════════════════════════════════════════════════════
 * 5. PUSH-PULL PWM — DUTY CYCLE LIMITS
 *
 *    duty_ticks controls BOTH Q1 ON-time and Q2 ON-time equally,
 *    ensuring volt-second balance across the transformer.
 *
 *    Dead-time at each gap = (PWM_HALF_PERIOD_TICKS - duty_ticks) × 0.125 µs
 *    At max duty (128 ticks): dead-time = (160-128) × 0.125 = 4 µs ✓
 *    At min duty ( 16 ticks): dead-time = (160-16)  × 0.125 = 18 µs
 *
 *    This gap is enforced by the compare hardware on every period —
 *    unlike the earlier ISR-driven scheme, it does not depend on
 *    interrupt response time, so the 4 µs figure at max duty is a
 *    hard guarantee, not a best-case estimate.
 *
 *    CONSTRAINT: duty_ticks must stay < PWM_HALF_PERIOD_TICKS (160), or
 *    the dead-time gap collapses to zero/negative and both channels'
 *    pulses can overlap — shoot-through. MAX_TICKS = 128 keeps a solid
 *    32-tick (4 µs) margin below that limit and enforces 16 µs maximum
 *    ON-time per the Rev E spec.
 * ════════════════════════════════════════════════════════════════════ */
#define PWM_DUTY_MIN_TICKS      16          /*  2 µs ON  (pot fully CCW)        */
#define PWM_DUTY_MAX_TICKS      128         /* 16 µs ON  (pot fully CW, Rev E)  */

/* ════════════════════════════════════════════════════════════════════
 * 6. POTENTIOMETER ADC INPUT
 *
 *    200-ohm pot wiper → PA0 (ADC1, Channel 0)
 *    Pot between 3.3V and GND; wiper to PA0.
 *    ADC result: 0 (pot CCW, 0V) to 4095 (pot CW, 3.3V)
 * ════════════════════════════════════════════════════════════════════ */
#define POT_PORT                GPIOA
#define POT_PIN                 0
#define POT_ADC_CHANNEL         0

#endif /* HW_CONFIG_H */