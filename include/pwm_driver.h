/*
 * pwm_driver.h — Push-Pull PWM Driver Interface
 *
 * Generates the two-phase push-pull switching waveform for Stage 1
 * of the DC-DC converter using TIM4 on PB6 and PB7.
 *
 * Architecture:
 *   TIM4 runs in CENTER-ALIGNED counting mode (CNT sweeps 0→ARR→0).
 *   Both channels are genuine hardware PWM outputs — no ISR, no software
 *   GPIO toggling, no interrupt-latency race in the gate-drive path.
 *
 *   PB6 (Q1): TIM4 CH1, PWM Mode 1 (HIGH while CNT < CCR1).
 *             Pulse of width 2×CCR1, centered on the trough (CNT=0).
 *
 *   PB7 (Q2): TIM4 CH2, PWM Mode 2 (HIGH while CNT >= CCR2).
 *             Pulse of width 2×(ARR-CCR2), centered on the peak (CNT=ARR),
 *             i.e. exactly half a period away from Q1's pulse.
 *
 * Dead-time guarantee:
 *   Gap 1 (Q1 fall → Q2 rise): (PWM_HALF_PERIOD_TICKS - duty) × 0.125 µs
 *   Gap 2 (Q2 fall → Q1 rise): (PWM_HALF_PERIOD_TICKS - duty) × 0.125 µs [symmetric]
 *   At max duty (128 ticks): both gaps = 4 µs ✓ — and, unlike a software-
 *   driven edge, that 4 µs is guaranteed by compare hardware on every
 *   single period, not contingent on interrupt response time.
 */

#ifndef PWM_DRIVER_H
#define PWM_DRIVER_H

#include "stm32f1xx.h"
#include <stdint.h>

/*
 * pwm_pushpull_init — Configure TIM4 and GPIO for push-pull operation.
 *
 * After this call:
 *   - PB6 and PB7 both produce hardware PWM at 25 kHz (center-aligned),
 *     duty = PWM_DUTY_MIN_TICKS (safe startup)
 *   - Both outputs are non-overlapping by construction of the compare
 *     hardware itself (dead-time guaranteed every period, not just on
 *     average — see pwm_driver.c for the center-aligned mechanism)
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
 * Q1 pulse: centered on the trough (CNT=0 / period boundary)
 * Q2 pulse: centered on the peak (CNT=ARR), exactly half a period away
 */
void pwm_set_duty(uint16_t duty_ticks);

#endif /* PWM_DRIVER_H */