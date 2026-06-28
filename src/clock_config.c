#include "clock_config.h"

void clock_init_hse(void) {
    RCC->CR |= RCC_CR_HSEON; // telling the chip "please start oscillating the external crystal." 

    while (!(RCC->CR & RCC_CR_HSERDY)); // Deliberately wait until the external crystal is stable and ready to use.
    RCC->CFGR &= ~RCC_CFGR_SW; // This clears the BIT field before writing a new value to it.
    RCC->CFGR |=  RCC_CFGR_SW_HSE; // This is the real switching of the system clock to the external crystal. The chip will now use the external crystal as its main clock source.

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE); // Loop until the system clock source is confirmed to be the external crystal.

    SystemCoreClock = 8000000UL; // Update the SystemCoreClock variable to reflect the new clock frequency (8 MHz in this case).
}