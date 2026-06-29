#include "gpio_driver.h"

//It enables the clock for the specified GPIO port (e.g., GPIOA, GPIOB, etc.) by setting the corresponding bit in the RCC->APB2ENR register. 
//This is necessary before configuring or using any GPIO pins on that port.
static void gpio_clock_enable(GPIO_TypeDef *port) {
    if (port == GPIOA) {
        RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    } else if (port == GPIOB) {
        RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    } else if (port == GPIOC) {
        RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    } else if (port == GPIOD) {
        RCC->APB2ENR |= RCC_APB2ENR_IOPDEN;
    }
}

void gpio_output_init(GPIO_TypeDef *port, uint8_t pin) {
    gpio_clock_enable(port); // Enable the clock for the specified GPIO port

    if (pin < 8) {
        port->CRL &= ~(0xF << (4 * pin));
        port->CRL |=  (0x1 << (4 * pin));
    } else {
        port->CRH &= ~(0xF << (4 * (pin - 8)));
        port->CRH |=  (0x1 << (4 * (pin - 8)));
    }
}

void gpio_af_output_init(GPIO_TypeDef *port, uint8_t pin) {
    gpio_clock_enable(port);

    if (pin < 8) {
        port->CRL &= ~(0xF << (4 * pin));
        port->CRL |=  (0xB << (4 * pin));
    } else {
        port->CRH &= ~(0xF << (4 * (pin - 8)));
        port->CRH |=  (0xB << (4 * (pin - 8)));
    }
}

void gpio_write(GPIO_TypeDef *port, uint8_t pin, uint8_t state) {
    if (state) {
        port->BSRR = (1 << pin);
    } else {
        port->BSRR = (1 << (pin + 16));
    }
}

void gpio_toggle(GPIO_TypeDef *port, uint8_t pin) {
    port->ODR ^= (1 << pin);
}