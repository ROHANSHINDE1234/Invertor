#include "hw_config.h"
#include "gpio_driver.h"

void delay(volatile uint32_t count) {
    while (count--);
}

int main(void) {
    gpio_output_init(LED_PORT, LED_PIN);

    while (1) {
        gpio_toggle(LED_PORT, LED_PIN);
        delay(500000);
    }
}