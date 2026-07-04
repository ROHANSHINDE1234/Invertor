/*
 * adc_driver.c — ADC Driver Implementation
 *
 * Configures ADC1 in continuous conversion mode on Channel 0 (PA0).
 * Continuous mode is used rather than software-triggered single conversion
 * because it avoids SWSTART trigger timing dependencies and always has
 * a fresh result ready when adc_read() is called.
 *
 * Pin: PA0 → ADC1 Channel 0 (200-ohm pot wiper)
 * ADC clock: PCLK2 / 6 = 8 MHz / 6 = 1.33 MHz (limit: 14 MHz)
 * Sampling time: 239.5 cycles → conversion time ≈ 180 µs (accuracy over speed)
 */

#include "adc_driver.h"
#include "hw_config.h"

void adc_init(void)
{
    /* 1. Enable clocks: ADC1 and GPIOA are both on APB2 */
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    /* 2. Configure PA0 as analog input.
     * CRL[3:0] = 0000: MODE=00 (input), CNF=00 (analog).
     * This disconnects the Schmitt trigger and connects the pin to the ADC MUX. */
    GPIOA->CRL &= ~(0xFu << (4u * POT_PIN));

    /* 3. ADC clock prescaler: PCLK2 / 6 = 8 MHz / 6 ≈ 1.33 MHz.
     * Must be ≤ 14 MHz per the STM32F103 datasheet. */
    RCC->CFGR &= ~RCC_CFGR_ADCPRE;
    RCC->CFGR |=  RCC_CFGR_ADCPRE_DIV6;

    /* 4. Set conversion sequence: one conversion, channel POT_ADC_CHANNEL.
     * SQR1: L[3:0] = 0 → 1 conversion total.
     * SQR3: first (only) conversion = channel 0. */
    ADC1->SQR1 = 0u;
    ADC1->SQR3 = POT_ADC_CHANNEL;

    /* 5. Maximum sampling time on channel 0 (239.5 ADC clock cycles).
     * Longer sampling → more settling time → more accurate reading from
     * the high-impedance pot wiper. Fine for a slow-moving control knob. */
    ADC1->SMPR2 |= (0x7u << (3u * POT_ADC_CHANNEL));

    /* 6. Enable continuous conversion mode and power on the ADC.
     * CONT: after each conversion, automatically start the next.
     * ADON (first write): exits power-down mode, starts stabilization. */
    ADC1->CR2 = ADC_CR2_CONT | ADC_CR2_ADON;

    /* 7. Wait for ADC stabilization (tSTAB ≥ 1 µs per datasheet).
     * 1000 iterations at 8 MHz ≈ 125 µs — well above the minimum. */
    for (volatile uint32_t i = 0u; i < 1000u; i++);

    /* 8. Reset calibration registers.
     * Mandatory before calibration on STM32F1. Clears any leftover
     * offset from a previous power cycle or corrupted state. */
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL);

    /* 9. Run self-calibration.
     * Hardware measures the internal offset and programs correction registers.
     * Without this, ADC accuracy on STM32F1 is degraded. */
    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL);

    /* 10. Start the first conversion.
     * On STM32F103: when ADON is already set, writing ADON=1 again
     * starts conversion. In continuous mode this begins an endless chain. */
    ADC1->CR2 |= ADC_CR2_ADON;
}

uint16_t adc_read(void)
{
    /* Wait for End-Of-Conversion flag (set by hardware when conversion completes) */
    while (!(ADC1->SR & ADC_SR_EOC));

    /* Clear EOC flag before reading DR.
     * In continuous mode, EOC stays set after conversion. Clearing it here
     * ensures the next call to adc_read() waits for a FRESH conversion result
     * rather than returning the same result again immediately. */
    ADC1->SR &= ~ADC_SR_EOC;

    /* Return lower 12 bits of data register (bits [15:12] are always zero) */
    return (uint16_t)(ADC1->DR & 0x0FFFu);
}
