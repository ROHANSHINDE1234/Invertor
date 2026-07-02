#include "hw_config.h"
#include "gpio_driver.h"
#include "clock_config.h"
#include "pwm_driver.h"
#include "adc_driver.h"

// Maps ADC reading (0-4095) to duty cycle ticks (PWM_DUTY_MIN to PWM_DUTY_MAX)
static uint16_t map_adc_to_duty(uint16_t adc_val) {
    uint32_t range = PWM_DUTY_MAX_TICKS - PWM_DUTY_MIN_TICKS;
    return (uint16_t)(PWM_DUTY_MIN_TICKS + (adc_val * range) / 4095);
}

void delay(volatile uint32_t count) {
    while (count--);
}

int main(void) {
    clock_init_hse();
    adc_init();             // configure ADC1 on PA0 (potentiometer)
    pwm_pushpull_init();

    gpio_output_init(LED_PORT, LED_PIN);

    while (1) {
        // gpio_toggle(LED_PORT, LED_PIN);
        // delay(500000);
        uint16_t adc_val   = adc_read();
        uint16_t duty      = map_adc_to_duty(adc_val);
        pwm_set_duty(duty);

        // Small delay between ADC reads to avoid updating the timer
        // registers faster than one switching period
        for (volatile uint32_t i = 0; i < 800; i++);
    }
}
// added from Office PC