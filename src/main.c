#include "hw_config.h"
#include "gpio_driver.h"
#include "clock_config.h"
#include "pwm_driver.h"

void delay(volatile uint32_t count) {
    while (count--);
}

int main(void) {
    clock_init_hse();
    pwm_pushpull_init();

    gpio_output_init(LED_PORT, LED_PIN);

    while (1) {
        // gpio_toggle(LED_PORT, LED_PIN);
        // delay(500000);
    }
}
// added from Office PC