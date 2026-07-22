# CLAUDE.md

Guidance for Claude Code when working in this repository.

## What this is

STM32F103C8T6 ("Blue Pill") bare-metal firmware for a two-stage inverter in a solar-powered LiFePO4 backup system: an **input-side push-pull** DC-DC stage (TIM4, PB6/PB7, 25kHz) and a **50Hz H-bridge output unfolder** (TIM3, PA4–PA7). Register-level C, CMSIS core + ST device headers, no HAL/LL library — everything touches peripheral registers directly (`RCC->...`, `TIM4->...`, `GPIOx->...`). Both stages are logic-analyzer verified (2026-07-22). See [README.md](README.md) for the full system/hardware picture; this file is about working in the firmware code.

## Build

Cross-compiles with `arm-none-eabi-gcc` via CMake (`CMakeLists.txt`), target Cortex-M3, no OS (`CMAKE_SYSTEM_NAME Generic`).

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

Output: `inverter_firmware.elf` / `.bin`, plus a printed `arm-none-eabi-size` summary (post-build step in CMakeLists.txt). There is no unit test suite — this is embedded firmware; correctness is verified on hardware (logic analyzer on PB6/PB7 for the push-pull, PA4–PA7 for the H-bridge) or by careful register-level reasoning.

Flashing/debugging is via OpenOCD + ST-Link, configured in `.vscode/launch.json` (cortex-debug extension), not from the command line in this repo.

## Code layout

- `include/hw_config.h` — **the single place for board-level constants**: pin assignments (`LED_PORT/PIN`, `PWM_PORT`, `PWM_PIN_Q1/Q2`, `HBRIDGE_PORT`, `HBRIDGE_PIN_HA/LA/HB/LB`, plus the now-unused `POT_PORT/PIN`, `POT_ADC_CHANNEL`) and timing (`PWM_PSC`, `PWM_HALF_PERIOD_TICKS`, `PWM_DUTY_MIN_TICKS`, `PWM_DUTY_MAX_TICKS`, `PWM_DUTY_FIXED_TICKS`, `HBRIDGE_TIM_PSC`, `HBRIDGE_ACTIVE_TICKS`, `HBRIDGE_DEAD_TICKS`). Change timing/pins here, not in the driver `.c` files.
- `src/clock_config.c` — HSE (external crystal) startup only. No PLL. `SYSCLK_HZ` = 8MHz.
- `src/gpio_driver.c` — minimal bit-banged register GPIO driver (no HAL). `gpio_output_init` = general-purpose push-pull output; `gpio_af_output_init` = alternate-function push-pull for timer PWM. `gpio_write` uses BSRR (atomic, ISR-safe); `gpio_toggle` uses ODR read-modify-write (not ISR-safe).
- `src/adc_driver.c` — ADC1 channel 0 on PA0, continuous conversion. **Was the duty-control pot reader; no longer called** (duty is fixed now) — kept in the tree for easy restoration, gc-sectioned out of the binary. Includes the mandatory STM32F1 `RSTCAL`/`CAL` sequence; single-conversion (`SQR1` L=0), so adding inputs needs a scan sequence.
- `src/pwm_driver.c` — TIM4 push-pull PWM, **center-aligned mode, both channels hardware-driven**: PB6 = CH1 (PWM mode 1, pulse centered on the count trough), PB7 = CH2 (PWM mode 2, pulse centered on the count peak, exactly half a period from PB6). No ISR in the gate-drive path — an earlier revision toggled PB7 from `TIM4_IRQHandler` on CC3/CC4 compare match, which raced the hardware-timed PB6 edge under interrupt latency and produced real overlap on the scope at high duty. `pwm_set_duty()` sets `CCR1 = half_on`, `CCR2 = PWM_HALF_PERIOD_TICKS - half_on` (both from `duty/2`), clamped to `[16..128]`.
- `src/hbridge_driver.c` — TIM3 50Hz H-bridge output unfolder. Four plain-GPIO gates (PA4=HA, PA5=LA, PA6=HB, PA7=LB) driven by a 4-phase state machine in `TIM3_IRQHandler`: POS (HA+LB) 9ms → all-off dead 1ms → NEG (HB+LA) 9ms → all-off dead 1ms. `ARR` is reprogrammed per phase (so ARPE is deliberately OFF); an all-off phase between every active phase is what prevents leg shoot-through. Not timing-critical (50Hz), so GPIO+ISR is fine here — unlike the push-pull.
- `src/main.c` — wiring only: `clock_init_hse()` → `pwm_pushpull_init()` → `pwm_set_duty(PWM_DUTY_FIXED_TICKS)` → `hbridge_init()` → idle. No pot/ADC, no control loop. Keep it that thin; no register access or hardware constants belong here.
- `include/CMSIS/`, `include/Device/` — vendor-provided (ARM CMSIS, ST device headers). Treat as read-only/generated; don't hand-edit.
- `startup/startup_stm32f103xb.s` — vendor-provided reset handler + vector table. Don't hand-edit unless adding a new interrupt handler symbol.
- `linker/STM32F103XB_FLASH.ld` — memory map for the STM32F103xB (128K flash / 20K RAM). Only touch if changing target chip or stack/heap sizing.

## Conventions to preserve

- **Direct register manipulation, no HAL/LL.** Follow the existing style (`PERIPH->REG |= FLAG;` with a comment explaining *why*, not what) rather than introducing STM32 HAL/LL calls.
- **Timing/pin constants belong in `hw_config.h`**, never hardcoded in driver source — this is the pattern the existing code already follows and should stay consistent.
- Driver modules are named `<peripheral>_driver.c/.h` (`gpio_driver`, `adc_driver`, `pwm_driver`, `hbridge_driver`) with an `.h` in `include/` mirroring the `.c` in `src/`. Follow this when adding a new peripheral driver. Remember to add new `.c` files to the `add_executable` list in `CMakeLists.txt`.
- The push-pull dead-time scheme (see README's PWM section) is **safety-critical**. Dead-time is derived, not fixed: each gap = `(PWM_HALF_PERIOD_TICKS - duty) × 0.125µs`, so it *shrinks as duty rises*. The invariants:
  - `PWM_DUTY_MAX_TICKS` (128) must stay **well below** `PWM_HALF_PERIOD_TICKS` (160) — at 160 the dead-time is zero and both MOSFETs' pulses can overlap (shoot-through).
  - `CCR1` and `CCR2` must always be derived from the *same* `duty/2` half-width (`CCR1 = half_on`, `CCR2 = PWM_HALF_PERIOD_TICKS - half_on`) — this is what keeps the two channels' pulses symmetric and non-overlapping. Never write them independently.
  - Both `OC1PE` and `OC2PE` preload bits in `pwm_pushpull_init()` are **not optional** — without them a mid-sweep `CCR1`/`CCR2` write applies immediately instead of at the next period boundary, which can shift an edge unpredictably mid-period. Same for `ARPE`.
  - **Do not reintroduce a software/ISR-driven channel on the push-pull.** An earlier revision drove PB7 from `TIM4_IRQHandler` on CC3/CC4 compare match instead of its native TIM4 CH2 hardware output; that raced against the hardware-timed PB6 edge with a margin that shrank to 4µs at max duty, and interrupt latency (worse at this project's `-O0` debug build) could exceed it — confirmed as real overlap on the scope. Center-aligned mode with both channels on real hardware PWM outputs (CH1/CH2) removes that failure mode entirely; keep it that way.

  Both the push-pull and the H-bridge were **logic-analyzer verified on 2026-07-22**: push-pull = 25kHz, 16µs ON, 4µs dead-time, 180°, no overlap; H-bridge = 50Hz, ~9ms active, ~1ms dead-time, correct `HA+LB`/`HB+LA` diagonals, no same-leg overlap. Prefer verifying any change on a logic analyzer over reasoning alone.

- The **H-bridge** (`hbridge_driver.c`) is the second safety-critical gate driver. Its invariants:
  - The **all-off DEAD phase between every active phase is mandatory** — it is the only thing preventing a leg (HA+LA or HB+LB) from being briefly shorted top-to-bottom during a polarity swap. Never let a POS phase transition directly to a NEG phase.
  - The two switches of one leg (`HA`/`LA`, or `HB`/`LB`) must never be commanded on simultaneously. The diagonals are `HA+LB` and `HB+LA`.
  - `ARR` is reprogrammed per phase inside the ISR, so **ARPE must stay OFF** on TIM3 (the new `ARR` must take effect for the period that just started, not the one after). This is the opposite of the push-pull's `ARPE`-on requirement — don't "fix" it to match.
  - Gate signals are **4 independent active-high 3.3V outputs**; if the physical gate-driver board expects one-PWM-per-leg or active-low, the driver needs reworking (see README bring-up notes).

## Non-code files (context only, don't parse/edit)

- `Pushpull_ckt.asc` — LTspice schematic of the push-pull power stage this firmware drives.
- `EE20_Transformer_Spec_RevE_FINAL.docx`, `Transformer_Design_Revision_History.docx` — transformer design specs for the same stage.

These explain *why* the PWM timing constants in `hw_config.h` are what they are, but are binary/schematic formats outside normal code review.

## Scope note

This repo currently contains only the inverter firmware — both stages (push-pull input + H-bridge output unfolder). The solar charger (CN3722 MPPT) and BMS front-end described in the README are still at the schematic/BOM planning stage and have no code yet — don't assume charger-side source files exist.
