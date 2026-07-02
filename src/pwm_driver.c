#include "pwm_driver.h"
#include "hw_config.h"
#include "gpio_driver.h"

void pwm_pushpull_init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;

    gpio_af_output_init(PWM_PORT, PWM_PIN_CH1);   // PB6, hardware PWM
    gpio_output_init(PWM_PORT, PWM_PIN_CH2);      // PB7, plain output (software-driven)
    gpio_write(PWM_PORT, PWM_PIN_CH2, 0);    // ADD: ensure PB7 starts LOW
    TIM4->PSC = 0;
    TIM4->ARR = PWM_ARR;

    TIM4->CCR1 = PWM_CCR1_FALL;
    TIM4->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM4->CCMR1 |=  (0x6 << 4);
    TIM4->CCMR1 |=  TIM_CCMR1_OC1PE;

    TIM4->CCER |= TIM_CCER_CC1E;
    TIM4->CCR3 = PWM_CCR3_RISE;
    TIM4->CCR4 = PWM_CCR4_FALL;

    TIM4->DIER |= TIM_DIER_CC3IE;
    TIM4->DIER |= TIM_DIER_CC4IE;

    NVIC_EnableIRQ(TIM4_IRQn);

    TIM4->CR1 |= TIM_CR1_ARPE;
    TIM4->CNT = 0;                           // ADD: ensure counter starts from 0
    TIM4->CR1 |= TIM_CR1_CEN;
}

void TIM4_IRQHandler(void) {
    if (TIM4->SR & TIM_SR_CC3IF) {
        TIM4->SR &= ~TIM_SR_CC3IF;
        gpio_write(PWM_PORT, PWM_PIN_CH2, 1);
    }

    if (TIM4->SR & TIM_SR_CC4IF) {
        TIM4->SR &= ~TIM_SR_CC4IF;
        gpio_write(PWM_PORT, PWM_PIN_CH2, 0);
    }
}

void pwm_set_duty(uint16_t duty_ticks) {
    // Clamp to safe range — enforces minimum dead-time on both sides
    if (duty_ticks < PWM_DUTY_MIN_TICKS) duty_ticks = PWM_DUTY_MIN_TICKS;
    if (duty_ticks > PWM_DUTY_MAX_TICKS) duty_ticks = PWM_DUTY_MAX_TICKS;

    // Update Q1's hardware PWM falling edge
    TIM4->CCR1 = duty_ticks;

    // Update Q2's software-driven falling edge (rising edge stays fixed at CCR3=160)
    TIM4->CCR4 = PWM_HALF_PERIOD + duty_ticks;
}