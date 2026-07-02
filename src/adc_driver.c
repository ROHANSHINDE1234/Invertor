#include "adc_driver.h"
#include "hw_config.h"

void adc_init(void) {
    // 1. Enable clocks for ADC1 and GPIOA
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // 2. Configure PA0 as analog input
    // CNF=00, MODE=00 clears all 4 bits for pin 0 in CRL
    GPIOA->CRL &= ~(0xF << (4 * POT_PIN));

    // 3. Set ADC clock prescaler to /2 (8MHz / 2 = 4MHz, within 14MHz max)
    RCC->CFGR &= ~RCC_CFGR_ADCPRE;
    RCC->CFGR |=  RCC_CFGR_ADCPRE_DIV2;

    // 4. Set longest sampling time for channel 0 (239.5 cycles)
    //    Longer sampling = more accurate, fine since we are reading a slow pot
    ADC1->SMPR2 |= (0x7 << (3 * POT_ADC_CHANNEL));

    // 5. Single conversion, channel 0 as first (and only) in sequence
    ADC1->SQR3 = POT_ADC_CHANNEL;
    ADC1->SQR1 = 0;

    // 6. Enable software trigger
    ADC1->CR2 |= ADC_CR2_EXTTRIG;
    ADC1->CR2 |= ADC_CR2_EXTSEL;   // EXTSEL = 111b = SWSTART

    // 7. Power up ADC
    ADC1->CR2 |= ADC_CR2_ADON;

    // 8. Wait for ADC to stabilize (at least 1us required)
    for (volatile uint32_t i = 0; i < 100; i++);

    // 9. Run calibration (mandatory on STM32F1 for accurate readings)
    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL);   // wait for calibration to complete
}

uint16_t adc_read(void) {
    // Start a single conversion
    ADC1->CR2 |= ADC_CR2_SWSTART;

    // Wait for end-of-conversion flag
    while (!(ADC1->SR & ADC_SR_EOC));

    // Return 12-bit result (0 to 4095)
    return (uint16_t)(ADC1->DR & 0x0FFF);
}