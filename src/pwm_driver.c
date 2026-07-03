#include "pwm_driver.h"
#include "hw_config.h"
#include "gpio_driver.h"

void pwm_pushpull_init(void) {

    /* TIM4 is on APB1 bus (different from GPIO's APB2) */
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;

    /* PB6: alternate-function output — TIM4 CH1 drives this pin directly */
    /* PB7: plain GPIO output — ISR writes this pin via gpio_write()      */
    gpio_af_output_init(PWM_PORT, PWM_PIN_CH1);
    gpio_output_init   (PWM_PORT, PWM_PIN_CH2);

    /* Force PB7 LOW before the timer starts — prevents overlap on first cycle */
    gpio_write(PWM_PORT, PWM_PIN_CH2, 0);

    /* ── Timebase: PSC=0 → one tick = 0.125 µs at 8 MHz ─────────── */
    TIM4->PSC = PWM_PSC;
    TIM4->ARR = PWM_ARR;

    /* ── CH1: Hardware PWM Mode 1 for Q1 (PB6) ──────────────────── */
    /* Output is HIGH while CNT < CCR1, LOW otherwise.               */
    /* OC1PE: preload enable — CCR1 updates only at period boundary, */
    /*         preventing a glitched short pulse mid-cycle.           */
    TIM4->CCR1   = PWM_CCR1_INIT;
    TIM4->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM4->CCMR1 |=  (0x6u << 4u);           /* PWM Mode 1 = 0b110           */
    TIM4->CCMR1 |=  TIM_CCMR1_OC1PE;        /* CCR1 preload enabled          */
    TIM4->CCER  |=  TIM_CCER_CC1E;           /* Connect CH1 to physical PB6   */

    /* ── CH3 & CH4: Silent compare sources for Q2 (PB7) ─────────── */
    /* No CCMR mode bits set, no CCER enable → no pin output at all. */
    /* These channels exist ONLY to fire interrupts at precise ticks. */
    /*                                                                 */
    /* CCR3 = 160 (fixed, 20 µs): ISR sets PB7 HIGH  ← never changes */
    /* CCR4 = 176–288 (dynamic):  ISR sets PB7 LOW   ← updated by ADC*/
    /*                                                                 */
    /* OC4PE (CCR4 preload): NEW FIX — prevents the race condition    */
    /* where writing CCR4 mid-period causes CC4 interrupt to be missed,*/
    /* leaving PB7 HIGH into the next period and overlapping PB6.     */
    TIM4->CCR3   = PWM_CCR3_RISE;            /* Fixed: Q2 rises at 20 µs     */
    TIM4->CCR4   = PWM_CCR4_INIT;            /* Startup: Q2 falls at 22 µs   */
    TIM4->CCMR2 |= TIM_CCMR2_OC4PE;         /* CCR4 preload — THE BUG FIX   */
    TIM4->DIER  |= TIM_DIER_CC3IE;           /* Enable CC3 interrupt          */
    TIM4->DIER  |= TIM_DIER_CC4IE;           /* Enable CC4 interrupt          */

    /* Enable TIM4 interrupt at the Cortex-M3 core level */
    NVIC_EnableIRQ(TIM4_IRQn);

    /* Reset counter to guarantee first cycle has correct phase */
    TIM4->CNT = 0u;

    /* Enable auto-reload preload, then start the counter */
    TIM4->CR1 |= TIM_CR1_ARPE;
    TIM4->CR1 |= TIM_CR1_CEN;               /* PWM starts here               */
}

void TIM4_IRQHandler(void) {
    /*
     * TIM4 shares one interrupt line for ALL events (CC1-CC4, update, etc.).
     * Always check WHICH flag caused this call before acting.
     * Always CLEAR the flag before returning — uncleared flags cause
     * the ISR to re-fire immediately after return (infinite loop).
     *
     * CC3 fires at CCR3=160 (tick 160 = 20 µs): Q2 (PB7) rising edge
     * CC4 fires at CCR4=160+duty (variable):     Q2 (PB7) falling edge
     *
     * Because CCR4 >= CCR3+16 always (clamped in pwm_set_duty), CC3
     * always fires BEFORE CC4 within the same period. No order conflict.
     */
    if (TIM4->SR & TIM_SR_CC3IF) {
        TIM4->SR &= ~TIM_SR_CC3IF;
        gpio_write(PWM_PORT, PWM_PIN_CH2, 1u);  /* PB7 HIGH at 20 µs */
    }
    if (TIM4->SR & TIM_SR_CC4IF) {
        TIM4->SR &= ~TIM_SR_CC4IF;
        gpio_write(PWM_PORT, PWM_PIN_CH2, 0u);  /* PB7 LOW  at 20+duty µs */
    }
}

void pwm_set_duty(uint16_t duty_ticks) {
    /*
     * Clamp to safe range before applying:
     *   MIN = 16 ticks → 2 µs ON,  18 µs dead-time (each side)
     *   MAX = 128 ticks → 16 µs ON,  4 µs dead-time (each side)
     *
     * CCR4 range with clamping: [176 .. 288] — always < ARR+1=320,
     * so CC4 always fires within the current period. No missed interrupts.
     */
    if (duty_ticks < PWM_DUTY_MIN_TICKS) duty_ticks = PWM_DUTY_MIN_TICKS;
    if (duty_ticks > PWM_DUTY_MAX_TICKS) duty_ticks = PWM_DUTY_MAX_TICKS;

    /* Q1: update falling edge (preload active — takes effect next period) */
    TIM4->CCR1 = duty_ticks;

    /*
     * Q2: update falling edge.
     * CCR3 (Q2 rising) = 160 — NEVER MODIFIED, so PB7 always starts at 20 µs.
     * CCR4 (Q2 falling) = 160 + duty — same ON-time as Q1, volt-second balance.
     *
     * With CCR4 preload enabled (OC4PE in CCMR2), this write is buffered and
     * takes effect at the NEXT period boundary (CNT wraps to 0). This eliminates
     * the race condition where writing CCR4 mid-period could cause a missed
     * CC4 interrupt and an incorrect extended-HIGH phase on PB7.
     */
    TIM4->CCR4 = PWM_HALF_PERIOD_TICKS + duty_ticks;
}