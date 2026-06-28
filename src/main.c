#include "hw_config.h"
#include "gpio_driver.h"
#include "clock_config.h"

void delay(volatile uint32_t count) {
    while (count--);
}

int main(void) {
    clock_init_hse();

    gpio_output_init(LED_PORT, LED_PIN);

    while (1) {
        gpio_toggle(LED_PORT, LED_PIN);
        delay(500000);
    }
}