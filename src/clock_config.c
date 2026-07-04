/*
 * clock_config.c — System Clock Implementation
 *
 * Switches the system clock from the default HSI (internal RC oscillator,
 * ±1% accuracy) to HSE (external crystal, ~20–50 ppm accuracy).
 *
 * Why this matters: at 25 kHz switching with 4 µs dead-time, a 1% clock
 * error shifts dead-time by 40 ns — acceptable, but HSE is free to use
 * and eliminates the concern entirely, especially as the board warms up.
 */

#include "clock_config.h"

void clock_init_hse(void)
{
    /* Step 1: Request external crystal oscillator to start.
     * HSEON does not start the oscillator instantly — it powers up the
     * crystal driver circuitry and begins the oscillation startup sequence. */
    RCC->CR |= RCC_CR_HSEON;

    /* Step 2: Wait until hardware confirms the crystal is stable.
     * The HSERDY flag is set automatically by hardware, not by software.
     * Skipping this wait risks running the core on an unstable clock.
     * If the crystal is absent, this loop hangs — an intentional diagnostic. */
    while (!(RCC->CR & RCC_CR_HSERDY));

    /* Step 3: Switch system clock source to HSE.
     * Clear SW field first (clean slate), then write HSE select pattern.
     * SW[1:0] = 01 selects HSE as SYSCLK source. */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |=  RCC_CFGR_SW_HSE;

    /* Step 4: Confirm the switch completed.
     * SW  = what was REQUESTED. SWS = what is ACTUALLY RUNNING.
     * The clock mux takes a few cycles to settle after the write. */
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);

    /* Step 5: Update CMSIS bookkeeping variable.
     * SystemCoreClock is a C variable, not a hardware register.
     * CMSIS does not automatically track manual clock changes, so
     * this must be updated by hand or debugger reads will be wrong. */
    SystemCoreClock = 8000000UL;
}
