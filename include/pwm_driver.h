/*
 * pwm_driver.h — Push-Pull PWM Driver Interface
 *
 * Generates the two-phase push-pull switching waveform for Stage 1
 * of the DC-DC converter using TIM4 on PB6 and PB7.
 *
 * Architecture:
 *   PB6 (Q1): TIM4 CH1 hardware PWM Mode 1.
 *             Output is HIGH while CNT < CCR1, LOW otherwise.
 *             Zero software involvement — peripheral-driven.
 *
 *   PB7 (Q2): Plain GPIO controlled by TIM4 CC3/CC4 compare interrupts.
 *             CCR3 = 160 (fixed, 20 µs): ISR sets PB7 HIGH  ← NEVER changes
 *             CCR4 = 160 + duty (dynamic): ISR sets PB7 LOW
 *             CCR4 preload (OC4PE) prevents race conditions on CCR4 writes.
 *
 * Dead-time guarantee:
 *   Gap 1 (Q1 fall → Q2 rise): (160 - duty) × 0.125 µs
 *   Gap 2 (Q2 fall → Q1 rise): (160 - duty) × 0.125 µs  [symmetric]
 *   At max duty (128 ticks): both gaps = 4 µs ✓
 *   CCR4 always in range [176..288] < ARR+1=320 — CC4 always fires within period.
 */

#ifndef PWM_DRIVER_H
#define PWM_DRIVER_H

#include "stm32f1xx.h"
#include <stdint.h>

/*
 * pwm_pushpull_init — Configure TIM4 and GPIO for push-pull operation.
 *
 * After this call:
 *   - PB6 produces hardware PWM at 25 kHz, duty = PWM_DUTY_MIN_TICKS (safe startup)
 *   - PB7 is driven by CC3/CC4 interrupts (same duty as PB6, starting at 20 µs)
 *   - Both outputs are non-overlapping (dead-time guaranteed by clamp in pwm_set_duty)
 *
 * The duty cycle immediately transitions to pot position on the first call
 * to pwm_set_duty() from the main application loop.
 */
void pwm_pushpull_init(void);

/*
 * pwm_set_duty — Update the ON-time for both Q1 and Q2.
 *
 *   duty_ticks: desired ON-time in timer ticks (0.125 µs each)
 *               Clamped internally to [PWM_DUTY_MIN_TICKS .. PWM_DUTY_MAX_TICKS]
 *               = [16 .. 128] ticks = [2 µs .. 16 µs]
 *
 * Both channels always have the same ON-time (volt-second balance).
 * Q1 rising edge: always at t=0 (period boundary, hardware)
 * Q2 rising edge: always at t=20 µs (tick 160, CCR3 never changes)
 */
void pwm_set_duty(uint16_t duty_ticks);

#endif /* PWM_DRIVER_H */