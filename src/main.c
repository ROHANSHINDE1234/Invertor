#include "hw_config.h"
#include "gpio_driver.h"
#include "clock_config.h"
#include "pwm_driver.h"
#include "adc_driver.h"

/*
 * map_adc_to_duty: Linear mapping from 12-bit ADC reading to duty-cycle ticks.
 *
 * ADC range:   0 (GND at PA0) to 4095 (3.3V at PA0)
 * Duty range:  PWM_DUTY_MIN_TICKS (2 µs) to PWM_DUTY_MAX_TICKS (16 µs)
 *
 * Formula: duty = MIN + (adc_val × (MAX - MIN)) / 4095
 */
static uint16_t map_adc_to_duty(uint16_t adc_val) {
    uint32_t range = (uint32_t)(PWM_DUTY_MAX_TICKS - PWM_DUTY_MIN_TICKS);
    return (uint16_t)(PWM_DUTY_MIN_TICKS + ((uint32_t)adc_val * range) / 4095u);
}

void delay(volatile uint32_t count) {
    while (count--);
}

int main(void) {
    clock_init_hse();     /* Switch to external 8 MHz crystal — must be first */
    adc_init();           /* Configure ADC1 on PA0 for potentiometer reading  */
    pwm_pushpull_init();  /* Start TIM4 push-pull PWM on PB6/PB7             */

    gpio_output_init(LED_PORT, LED_PIN);

    while (1) {
        // gpio_toggle(LED_PORT, LED_PIN);
        // delay(500000);
        uint16_t adc_val   = adc_read();
        uint16_t duty      = map_adc_to_duty(adc_val);
        pwm_set_duty(duty);

        /*
         * Wait at least one switching period (40 µs) before next update.
         * Even with CCR4 preload, updating faster than one period serves
         * no purpose and can cause the ADC to race ahead of the timer.
         * 800 iterations at 8 MHz ≈ 100 µs = 2.5 full switching periods.
         */

        // Small delay between ADC reads to avoid updating the timer
        // registers faster than one switching period
        for (volatile uint32_t i = 0; i < 800; i++);
    }
}
// added from Office PC