#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include "stm32f1xx.h"

//GPIO_TypeDef *port is the pointer to the GPIO port (e.g., GPIOA, GPIOB, etc.)
//uint8_t pin is the pin number (0-15) corresponding to the GPIO pin
void gpio_output_init(GPIO_TypeDef *port, uint8_t pin);
void gpio_toggle(GPIO_TypeDef *port, uint8_t pin);
void gpio_af_output_init(GPIO_TypeDef *port, uint8_t pin);
void gpio_write(GPIO_TypeDef *port, uint8_t pin, uint8_t state);

#endif