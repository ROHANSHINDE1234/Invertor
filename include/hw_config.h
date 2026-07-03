// Include guards these prevent the contents repetition if the file is been called form many files. This is a good practice to avoid redefinition errors.
#ifndef HW_CONFIG_H
#define HW_CONFIG_H

/*
 * hw_config.h — The ONLY file in this project that contains
 * hardware-specific definitions. If a pin, peripheral, or timing
 * constant changes, edit THIS file only. All drivers read from here.
 */

// ---- Onboard LED (Blue Pill) ----
#define LED_PORT        GPIOC // Port C
#define LED_PIN         13    // Pin 13

/* ── Push-Pull PWM Output Pins ────────────────────────────────── */
/*
 * TIM4 is on APB1 (not APB2 where GPIO and TIM1 live).
 * PB6 = TIM4 CH1 = Q1 (hardware PWM, zero jitter, alternate-function output)
 * PB7 = Q2 (plain GPIO output, driven by CC3/CC4 silent compare interrupts)
 */
// ---- Stage 1 Push-Pull PWM (TIM4, CH1=PB6, CH2=PB7) ----
#define PWM_PORT        GPIOB
#define PWM_PIN_CH1     6
#define PWM_PIN_CH2     7

#define PWM_TIMER       TIM4

/* ── Clock & Switching Frequency ──────────────────────────────── */
#define PWM_FREQ_HZ     25000UL
#define SYSCLK_HZ       8000000UL

/* ── Timer Register Constants ─────────────────────────────────── */
/*
 * All values derived from:
 *   tick period = 1 / SYSCLK_HZ = 0.125 µs  (PSC = 0, no prescaling)
 *   full period = 40 µs / 0.125 µs = 320 ticks  →  ARR = 319
 *   half-period = 20 µs / 0.125 µs = 160 ticks  (Q2 always starts here)
 *
 * Startup values (CCR1_INIT, CCR4_INIT) are immediately overwritten
 * by the ADC control loop in the first iteration of main(). They are
 * defined here only to give the timer a safe initial state before
 * the ADC reads the potentiometer for the first time.
 */
// ---- Push-pull timing, derived from 8MHz clock, 40us period ----
#define PWM_PSC         0           /* No prescaling: tick = 0.125 µs   */
#define PWM_ARR         319    // 320 ticks = 40us period (25kHz)
#define PWM_CCR1_INIT           16          /* Safe startup: Q1 ON for 2 µs     */
#define PWM_CCR3_RISE           160         /* Q2 ALWAYS rises at 20 µs (FIXED) */
#define PWM_CCR4_INIT           176         /* Safe startup: Q2 falls at 22 µs  */
/*                                           CCR4_INIT = CCR3 + CCR1_INIT       */

// #define PWM_CCR1_FALL   60    // Q1 (PB6) falls at 16us
// #define PWM_CCR3_RISE   160    // Q2 (PB7) rises at 20us
// #define PWM_CCR4_FALL   220    // Q2 (PB7) falls at 36us


/* ── Potentiometer ADC Input ───────────────────────────────────── */
/* 200-ohm pot wiper → PA0 (ADC1, Channel 0)                       */
#define POT_PORT            GPIOA
#define POT_PIN             0
#define POT_ADC_CHANNEL     0

/* ── Potentiometer Duty Cycle Limits ──────────────────────────── */
/*
 * Duty cycle is the ON-time for both Q1 and Q2 (always equal, for
 * volt-second balance). CCR3 = 160 is fixed. Only CCR1 and CCR4 move.
 *
 * CCR1 range:  [PWM_DUTY_MIN_TICKS .. PWM_DUTY_MAX_TICKS]
 * CCR4 range:  [CCR3 + MIN         .. CCR3 + MAX        ]
 *           =  [176                .. 288               ]
 *
 * Dead-time at each gap:
 *   Gap 1: CCR3 - CCR1 = 160 - duty  →  at max duty: 32 ticks = 4 µs
 *   Gap 2: ARR+1 - CCR4 = 320 - (160+duty)  →  same = 4 µs at max
 *
 * CONSTRAINT: CCR4 must always be < ARR+1 = 320
 *   160 + PWM_DUTY_MAX_TICKS < 320  →  MAX_TICKS < 160
 *   128 satisfies this with a 32-tick (4 µs) safety margin.
 */
#define PWM_HALF_PERIOD_TICKS   160         /* 20 µs in ticks — Q2 rise (FIXED) */
#define PWM_DUTY_MIN_TICKS      16          /*  2 µs ON time (minimum, pot CCW) */
#define PWM_DUTY_MAX_TICKS      128         /* 16 µs ON time (maximum, pot CW)  */




#endif