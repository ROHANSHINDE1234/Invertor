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

// ---- Push-pull timing, derived from 8MHz clock, 40us period ----
#define PWM_ARR         319    // 320 ticks = 40us period (25kHz)
#define PWM_CCR1_FALL   128    // Q1 (PB6) falls at 16us
#define PWM_CCR3_RISE   160    // Q2 (PB7) rises at 20us
#define PWM_CCR4_FALL   288    // Q2 (PB7) falls at 36us


#endif