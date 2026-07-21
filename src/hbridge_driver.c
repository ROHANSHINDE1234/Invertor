/*
 * hbridge_driver.c — H-Bridge Output Unfolder Implementation
 *
 * ──────────────────────────────────────────────────────────────────
 * 50 Hz SQUARE-WAVE UNFOLD, driven by a 4-phase TIM3 state machine:
 *
 *   phase 0  POS : HA + LB conduct   (current A → load → B)   9 ms
 *   phase 1  DEAD: all four off                               1 ms
 *   phase 2  NEG : HB + LA conduct   (current B → load → A)   9 ms
 *   phase 3  DEAD: all four off                               1 ms
 *                                          period = 20 ms = 50 Hz
 *
 * TIM3 ticks at 1 µs (PSC=7). ARR is reprogrammed each phase to the phase
 * duration; the update interrupt advances the state machine.
 *
 * WHY THE ALL-OFF DEAD-TIME MATTERS:
 *   Going straight from POS (HA+LB) to NEG (HB+LA) would, for one instant,
 *   risk HA and LA (leg A) or HB and LB (leg B) both being on if the turn-off
 *   lagged the turn-on — a top-to-bottom short across the DC rail. Passing
 *   through an explicit all-off phase between every active phase removes that
 *   possibility entirely: the two switches of a leg are never commanded on in
 *   adjacent phases. 1 ms is enormous relative to MOSFET switching, deliberately
 *   generous for a first bench trial (tighten HBRIDGE_DEAD_TICKS later).
 *
 * WHY PLAIN GPIO + ISR IS FINE HERE:
 *   Unlike the 25 kHz push-pull (which is pure hardware PWM precisely because
 *   4 µs edges cannot tolerate interrupt latency), this stage switches at
 *   50 Hz with millisecond-scale dead-time. A few microseconds of ISR jitter
 *   is utterly negligible, so a simple software state machine is the clearest
 *   correct implementation.
 * ──────────────────────────────────────────────────────────────────
 */

#include "hbridge_driver.h"
#include "hw_config.h"
#include "gpio_driver.h"

/* Current phase of the unfolder state machine (0..3, see file header). */
static volatile uint8_t hb_phase;

/* Drive all four gates LOW — the dead-time / safe state. */
static void hbridge_all_off(void)
{
    gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_HA, 0u);
    gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_LA, 0u);
    gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_HB, 0u);
    gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_LB, 0u);
}

/*
 * hbridge_apply_phase — Set the four gates for phase p and load TIM3->ARR
 * with that phase's duration.
 *
 * For the active phases, the opposite-leg partner gates are explicitly driven
 * LOW *before* the diagonal pair is driven HIGH. Because every active phase is
 * preceded by an all-off DEAD phase they are already low, but writing them
 * again makes each transition self-contained and impossible to misread: within
 * any single leg only one switch is ever commanded on.
 */
static void hbridge_apply_phase(uint8_t p)
{
    switch (p) {
    case 0: /* POS: diagonal HA + LB */
        gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_HB, 0u);   /* leg B: low side off first */
        gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_LA, 0u);   /* leg A: low side off first */
        gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_HA, 1u);   /* leg A high side on         */
        gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_LB, 1u);   /* leg B low  side on         */
        TIM3->ARR = HBRIDGE_ACTIVE_TICKS;
        break;

    case 2: /* NEG: diagonal HB + LA */
        gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_HA, 0u);   /* leg A: high side off first */
        gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_LB, 0u);   /* leg B: low  side off first */
        gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_HB, 1u);   /* leg B high side on         */
        gpio_write(HBRIDGE_PORT, HBRIDGE_PIN_LA, 1u);   /* leg A low  side on         */
        TIM3->ARR = HBRIDGE_ACTIVE_TICKS;
        break;

    default: /* phases 1 and 3: dead-time, everything off */
        hbridge_all_off();
        TIM3->ARR = HBRIDGE_DEAD_TICKS;
        break;
    }
}

void hbridge_init(void)
{
    /* ── 1. Enable TIM3 clock (APB1, same bus as TIM4) ─────────────── */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* ── 2. Four gate pins as general-purpose push-pull outputs, LOW ── */
    gpio_output_init(HBRIDGE_PORT, HBRIDGE_PIN_HA);
    gpio_output_init(HBRIDGE_PORT, HBRIDGE_PIN_LA);
    gpio_output_init(HBRIDGE_PORT, HBRIDGE_PIN_HB);
    gpio_output_init(HBRIDGE_PORT, HBRIDGE_PIN_LB);
    hbridge_all_off();

    /* ── 3. Timebase: 1 µs per tick ─────────────────────────────────
     * ARR is written per-phase in the ISR, so ARPE is intentionally left
     * DISABLED: each new ARR must take effect for the period that just
     * started at this update, not the one after. Force an update event now
     * to load the PSC prescaler shadow, then clear the flag it sets so the
     * first real interrupt only comes after a full POS phase. */
    TIM3->PSC = HBRIDGE_TIM_PSC;
    TIM3->ARR = HBRIDGE_ACTIVE_TICKS;
    TIM3->CNT = 0u;
    TIM3->EGR |= TIM_EGR_UG;              /* preload PSC, reset counter        */
    TIM3->SR  &= ~TIM_SR_UIF;             /* clear the UG-generated update flag */

    /* ── 4. Apply the first phase (POS conduction) ─────────────────── */
    hb_phase = 0u;
    hbridge_apply_phase(0u);

    /* ── 5. Enable the update interrupt and start ───────────────────
     * TIM3 is the only interrupt in the system (the push-pull is pure
     * hardware PWM), and it is not timing-critical, so default priority is
     * fine. CEN last — the unfolder free-runs from here. */
    TIM3->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CR1  |= TIM_CR1_CEN;
}

/*
 * TIM3_IRQHandler — advance the unfolder state machine on each update event.
 *
 * The vector table symbol is provided (weakly) by startup_stm32f103xb.s;
 * defining it here overrides the default handler. Keep the body short: clear
 * the flag first, then advance and apply the next phase (which also reloads
 * ARR for that phase's duration).
 */
void TIM3_IRQHandler(void)
{
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR &= ~TIM_SR_UIF;                 /* clear before acting        */
        hb_phase = (uint8_t)((hb_phase + 1u) & 0x3u);
        hbridge_apply_phase(hb_phase);
    }
}
