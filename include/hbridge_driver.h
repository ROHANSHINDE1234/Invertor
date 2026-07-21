/*
 * hbridge_driver.h — H-Bridge Output Unfolder Interface
 *
 * Stage 2 of the inverter. The push-pull + transformer + rectifier produce a
 * high-voltage DC rail; this full bridge unfolds that rail into 50 Hz AC by
 * alternating its two diagonal switch pairs.
 *
 * Architecture:
 *   TIM3 drives a 4-phase state machine from its update interrupt:
 *       POS active (HA+LB)  → 9 ms   (positive half-cycle)
 *       dead-time (all off) → 1 ms
 *       NEG active (HB+LA)  → 9 ms   (negative half-cycle)
 *       dead-time (all off) → 1 ms
 *   = 20 ms period = 50 Hz square wave.
 *
 *   The four gates (HA/LA/HB/LB) are plain GPIO on PA4–PA7, written from the
 *   ISR. An all-off dead-time state is inserted at EVERY polarity swap, so the
 *   bridge is never shorted top-to-bottom while a leg changes state. Pins and
 *   timing live in hw_config.h (section 7).
 *
 * This stage is independent of the push-pull: they run on separate timers
 * (TIM3 vs TIM4) and do not need to be phase-locked for a bench trial.
 */

#ifndef HBRIDGE_DRIVER_H
#define HBRIDGE_DRIVER_H

#include "stm32f1xx.h"
#include <stdint.h>

/*
 * hbridge_init — Configure the four gate pins and start the 50 Hz unfolder.
 *
 * After this call the bridge free-runs: TIM3 sequences the diagonal pairs with
 * a dead-time gap at each swap, with no further CPU involvement beyond the
 * short update ISR. All four gates are forced LOW before the timer starts.
 */
void hbridge_init(void);

#endif /* HBRIDGE_DRIVER_H */
