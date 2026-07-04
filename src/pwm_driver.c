/*
 * pwm_driver.c — Push-Pull PWM Implementation
 *
 * ──────────────────────────────────────────────────────────────────
 * WAVEFORM (at max duty, CCR1=128):
 *
 *   t (µs):  0    16   20   36   40
 *            |    |    |    |    |
 *   PB6 Q1:  ‾‾‾‾‾____|____|____|‾‾‾  (hardware PWM, zero jitter)
 *   PB7 Q2:  _____|____|‾‾‾‾‾____|___  (ISR-driven, 20µs fixed start)
 *                 ^4µs ^    ^4µs
 *               dead  Q2 ON  dead
 *
 * ──────────────────────────────────────────────────────────────────
 * WHY CCR4 PRELOAD (OC4PE) IS CRITICAL:
 *
 *   Without preload, writing TIM4->CCR4 takes effect IMMEDIATELY.
 *   If pwm_set_duty() writes a new CCR4 value and the timer counter
 *   has already passed that tick, the CC4 interrupt is silently missed
 *   for the current period. PB7 stays HIGH into the next period and
 *   overlaps with PB6 — confirmed shoot-through on the logic analyzer.
 *
 *   With OC4PE enabled, CCR4 writes are buffered and only applied
 *   at the next period boundary (CNT wraps to 0). No mid-period glitch.
 *
 * Logic analyzer CSV confirmed: without this bit, CC4 fires at wrong
 * ticks (8 instead of 256), causing 1µs overlap every period.
 * ──────────────────────────────────────────────────────────────────
 */

#include "pwm_driver.h"
#include "hw_config.h"
#include "gpio_driver.h"

void pwm_pushpull_init(void)
{
    /* ── 1. Enable TIM4 clock ────────────────────────────────────────
     * TIM4 is on APB1 (address 0x40000800).
     * GPIO and TIM1 are on APB2 — different bus, different enable register. */
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;

    /* ── 2. Configure output pins ────────────────────────────────────
     * PB6 (Q1): alternate-function — TIM4 CH1 hardware drives this pin.
     * PB7 (Q2): plain GPIO output — the ISR drives this pin via gpio_write(). */
    gpio_af_output_init(PWM_PORT, PWM_PIN_Q1);
    gpio_output_init   (PWM_PORT, PWM_PIN_Q2);

    /* Force PB7 LOW before the timer starts.
     * Without this, PB7 could be HIGH from a previous run or boot state,
     * causing overlap with PB6 on the very first switching cycle. */
    gpio_write(PWM_PORT, PWM_PIN_Q2, 0u);

    /* ── 3. Timebase ─────────────────────────────────────────────────
     * PSC = 0  → no prescaling → tick = 1 / 8 MHz = 0.125 µs
     * ARR = 319 → period = 320 ticks = 40 µs = 25 kHz */
    TIM4->PSC = PWM_PSC;
    TIM4->ARR = PWM_ARR;

    /* ── 4. CH1: Hardware PWM Mode 1 for Q1 (PB6) ───────────────────
     * PWM Mode 1 (OC1M = 0b110):
     *   Output HIGH while CNT < CCR1, LOW otherwise.
     *   → PB6 ON from tick 0 to CCR1, then LOW until period wraps.
     *
     * OC1PE (preload): CCR1 updates take effect at the next period
     *   boundary, not immediately. Prevents a short glitch if CCR1 is
     *   written while the counter is near the compare point.
     *
     * CC1E: connects CH1 output logic to the physical PB6 pin.
     *   Without this bit, the PWM waveform is computed internally
     *   but never reaches the pin. */
    TIM4->CCR1   = PWM_DUTY_MIN_TICKS;       /* Safe startup duty             */
    TIM4->CCMR1 &= ~TIM_CCMR1_OC1M;          /* Clear mode bits               */
    TIM4->CCMR1 |=  (0x6u << 4u);             /* PWM Mode 1 = 0b110            */
    TIM4->CCMR1 |=  TIM_CCMR1_OC1PE;          /* CCR1 preload enable           */
    TIM4->CCER  |=  TIM_CCER_CC1E;            /* Connect CH1 to PB6 pin        */

    /* ── 5. CH3: Fixed compare point for Q2 rising edge ─────────────
     * CCR3 = PWM_CCR3_RISE = 160 (20 µs): ISR sets PB7 HIGH.
     * This value is NEVER modified at runtime. PB7 always starts at 20 µs.
     *
     * No CCMR2 mode bits set, no CCER enable — CH3 has no pin output.
     * It exists only to trigger an interrupt at tick 160. */
    TIM4->CCR3  = PWM_CCR3_RISE;              /* Fixed: Q2 always ON at 20 µs  */
    TIM4->DIER |= TIM_DIER_CC3IE;             /* Enable CC3 interrupt           */

    /* ── 6. CH4: Dynamic compare point for Q2 falling edge ──────────
     * CCR4 = PWM_HALF_PERIOD_TICKS + duty: ISR sets PB7 LOW.
     * Updated by pwm_set_duty() when the pot changes.
     *
     * OC4PE (CCR4 preload): THE CRITICAL BUG FIX.
     *   Buffers CCR4 writes so they only apply at the next period boundary.
     *   Without this, writing CCR4 mid-period (when CNT has already passed
     *   the new value) causes CC4 to be missed, leaving PB7 HIGH into the
     *   next period and causing shoot-through confirmed by logic analyzer. */
    TIM4->CCR4  = PWM_HALF_PERIOD_TICKS + PWM_DUTY_MIN_TICKS; /* Startup      */
    TIM4->CCMR2 |= TIM_CCMR2_OC4PE;          /* CCR4 preload — BUG FIX        */
    TIM4->DIER  |= TIM_DIER_CC4IE;            /* Enable CC4 interrupt           */

    /* ── 7. Enable TIM4 interrupt at the Cortex-M3 core ─────────────
     * DIER tells TIM4 to raise its hand; NVIC_EnableIRQ tells the core
     * to listen. Both steps are required for the ISR to execute. */
    NVIC_EnableIRQ(TIM4_IRQn);

    /* ── 8. Reset counter and start ─────────────────────────────────
     * CNT = 0 ensures the first period starts with correct phase.
     * Without this, CNT may contain a leftover value, shifting all
     * compare-match events by an unpredictable offset on the first cycle.
     *
     * ARPE: ARR preload. Like OC1PE/OC4PE but for the period register.
     * CEN:  counter enable — PWM starts here. */
    TIM4->CNT = 0u;
    TIM4->CR1 |= TIM_CR1_ARPE;
    TIM4->CR1 |= TIM_CR1_CEN;
}

/*
 * TIM4_IRQHandler — Interrupt service routine for TIM4 events.
 *
 * TIM4 uses ONE shared interrupt line for ALL events (CC1–CC4, update, etc.).
 * The SR (Status Register) flags identify which event(s) caused this call.
 *
 * Rules observed here:
 *   1. Always check SR flags before acting — never assume which event fired.
 *   2. Always clear each flag before the gpio_write — uncleared flags cause
 *      the ISR to re-fire immediately after return (priority-level infinite loop).
 *   3. CC3 fires first (tick 160), then CC4 (tick 176–288). Because
 *      CCR4 ≥ CCR3+16 always (enforced by clamp in pwm_set_duty), they
 *      never occur simultaneously. No order ambiguity.
 */
void TIM4_IRQHandler(void)
{
    /* CC3 match at tick 160 (20 µs): Q2 (PB7) rising edge */
    if (TIM4->SR & TIM_SR_CC3IF) {
        TIM4->SR &= ~TIM_SR_CC3IF;             /* clear BEFORE gpio write       */
        gpio_write(PWM_PORT, PWM_PIN_Q2, 1u);  /* PB7 HIGH — Q2 turns ON        */
    }

    /* CC4 match at tick 160+duty (20+duty µs): Q2 (PB7) falling edge */
    if (TIM4->SR & TIM_SR_CC4IF) {
        TIM4->SR &= ~TIM_SR_CC4IF;             /* clear BEFORE gpio write       */
        gpio_write(PWM_PORT, PWM_PIN_Q2, 0u);  /* PB7 LOW  — Q2 turns OFF       */
    }
}

void pwm_set_duty(uint16_t duty_ticks)
{
    /* Enforce safe range.
     * MIN = 16: guarantees minimum 2 µs dead-time on each gap.
     * MAX = 128: enforces 16 µs maximum ON-time (Rev E specification).
     *   At MAX: CCR4 = 160+128 = 288 < ARR+1=320 → CC4 always fires in period. */
    if (duty_ticks < PWM_DUTY_MIN_TICKS) { duty_ticks = PWM_DUTY_MIN_TICKS; }
    if (duty_ticks > PWM_DUTY_MAX_TICKS) { duty_ticks = PWM_DUTY_MAX_TICKS; }

    /* Update Q1 falling edge (CCR1 preload active → takes effect next period) */
    TIM4->CCR1 = duty_ticks;

    /* Update Q2 falling edge.
     * CCR3 (Q2 rising) = 160, NEVER CHANGES → PB7 always starts at 20 µs.
     * CCR4 (Q2 falling) = 160 + duty → same ON-time as Q1 (volt-second balance).
     *
     * CCR4 preload (OC4PE) active → buffered, applies at next period boundary.
     * This is what prevents the race condition that caused shoot-through. */
    TIM4->CCR4 = PWM_HALF_PERIOD_TICKS + duty_ticks;
}
