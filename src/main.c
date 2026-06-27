#include "stm32f1xx.h"

void delay(volatile uint32_t count) {
    while (count--);
}

int main(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    GPIOC->CRH &= ~(0xF << 4*(13-8));
    GPIOC->CRH |=  (0x1 << 4*(13-8));
    while (1) {
        GPIOC->ODR ^= (1 << 13);
        delay(500000);
    }
}