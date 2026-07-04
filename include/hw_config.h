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
 *    PB7 = Q2: plain GPIO output, driven by CC3/CC4 compare interrupts
 * ════════════════════════════════════════════════════════════════════ */
#define PWM_PORT                GPIOB
#define PWM_PIN_Q1              6           /* PB6: hardware PWM (TIM4 CH1)     */
#define PWM_PIN_Q2              7           /* PB7: software ISR-driven GPIO     */

/* ════════════════════════════════════════════════════════════════════
 * 4. PUSH-PULL PWM — TIMER CONSTANTS
 *
 *    Switching frequency: 25 kHz  →  Period = 40 µs
 *    Timer prescaler (PSC): 0     →  1 tick = 1/8 MHz = 0.125 µs
 *
 *    Tick arithmetic:
 *      Ticks per period  = 40 µs / 0.125 µs = 320   →  ARR = 319
 *      Half-period ticks = 20 µs / 0.125 µs = 160   (Q2 always starts here)
 *
 *    Waveform targets (Rev E specification):
 *      Q1 (PB6): rises at tick 0 (period boundary, hardware)
 *                falls at tick CCR1 (set by pot control)
 *      Q2 (PB7): rises at tick 160 = 20 µs  ← ALWAYS FIXED, never changes
 *                falls at tick 160 + CCR1   (same ON-time as Q1)
 *
 *    Startup safe values (immediately overwritten by ADC on first loop):
 *      CCR1_STARTUP = PWM_DUTY_MIN_TICKS = minimum duty (safest state)
 *      CCR4_STARTUP = PWM_HALF_PERIOD_TICKS + PWM_DUTY_MIN_TICKS
 * ════════════════════════════════════════════════════════════════════ */
#define PWM_FREQ_HZ             25000UL
#define PWM_PSC                 0
#define PWM_ARR                 319         /* (SYSCLK_HZ / PWM_FREQ_HZ) - 1   */

#define PWM_HALF_PERIOD_TICKS   160         /* 20 µs — Q2 rise point, FIXED     */

/* CCR3 (Q2 rise): set once in init, NEVER modified at runtime.
 * If this value ever changes, PB7 will no longer start at 20 µs. */
#define PWM_CCR3_RISE           160         /* = PWM_HALF_PERIOD_TICKS          */

/* ════════════════════════════════════════════════════════════════════
 * 5. PUSH-PULL PWM — DUTY CYCLE LIMITS
 *
 *    duty_ticks controls BOTH Q1 ON-time and Q2 ON-time equally,
 *    ensuring volt-second balance across the transformer.
 *
 *    CCR1 = duty_ticks
 *    CCR4 = PWM_HALF_PERIOD_TICKS + duty_ticks
 *
 *    Dead-time at each gap = (PWM_HALF_PERIOD_TICKS - duty_ticks) × 0.125 µs
 *    At max duty (128 ticks): dead-time = (160-128) × 0.125 = 4 µs ✓
 *    At min duty ( 16 ticks): dead-time = (160-16)  × 0.125 = 18 µs
 *
 *    CONSTRAINT: CCR4 must always be < ARR+1 = 320
 *      CCR4_max = 160 + 128 = 288 < 320  ✓  (32-tick margin)
 *
 *    MAX_TICKS = 128 enforces 16 µs maximum ON-time per the Rev E spec.
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