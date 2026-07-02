// Include guards these prevent the contents repetition if the file is been called form many files. This is a good practice to avoid redefinition errors.
#ifndef HW_CONFIG_H
#define HW_CONFIG_H

// ---- Onboard LED (Blue Pill) ----
#define LED_PORT        GPIOC // Port C
#define LED_PIN         13    // Pin 13

// ---- Stage 1 Push-Pull PWM (TIM4, CH1=PB6, CH2=PB7) ----
#define PWM_PORT        GPIOB
#define PWM_PIN_CH1     6
#define PWM_PIN_CH2     7

#define PWM_TIMER       TIM4

#define PWM_FREQ_HZ     25000UL
#define SYSCLK_HZ       8000000UL

// // ---- Push-pull timing, derived from 8MHz clock, 40us period ----
// #define PWM_ARR         319    // 320 ticks = 40us period (25kHz)
// #define PWM_CCR1_FALL   128    // Q1 (PB6) falls at 16us
// #define PWM_CCR3_RISE   160    // Q2 (PB7) rises at 20us
// #define PWM_CCR4_FALL   288    // Q2 (PB7) falls at 36us


/*
t=0µs    Q1 rises (hardware, period boundary)
t=12µs   Q1 falls  (CCR1=60)
t=20µs   Q2 rises  (CCR3=160, ISR)   ← 10µs dead-time gap
t=32µs   Q2 falls  (CCR4=220, ISR)
t=40µs   wraps     ← 10µs dead-time gap
*/
// ---- Push-pull timing, derived from 8MHz clock, 40us period ----
#define PWM_ARR         319    // 320 ticks = 40us period (25kHz)
#define PWM_CCR1_FALL   60    // Q1 (PB6) falls at 16us
#define PWM_CCR3_RISE   160    // Q2 (PB7) rises at 20us
#define PWM_CCR4_FALL   220    // Q2 (PB7) falls at 36us

// ---- Potentiometer ADC input ----
#define POT_PORT            GPIOA
#define POT_PIN             0
#define POT_ADC_CHANNEL     0

// ---- Duty cycle limits (ticks, based on 8MHz / 25kHz / ARR=319) ----
// Half-period = 160 ticks = 20us
// Minimum dead-time enforced = 16 ticks = 2us on each side
#define PWM_DUTY_MIN_TICKS  16     // 10% of half-period = 2us ON
#define PWM_DUTY_MAX_TICKS  144    // 90% of half-period = 18us ON (2us dead-time)
#define PWM_HALF_PERIOD     160    // 20us in ticks, Q2 always rises here




#endif