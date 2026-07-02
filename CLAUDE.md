# CLAUDE.md

Guidance for Claude Code when working in this repository.

## What this is

STM32F103C8T6 ("Blue Pill") bare-metal firmware for the push-pull DC-DC inverter stage of a solar-powered LiFePO4 backup system. Register-level C, CMSIS core + ST device headers, no HAL/LL library — everything touches peripheral registers directly (`RCC->...`, `TIM4->...`, `GPIOx->...`). See [README.md](README.md) for the full system/hardware picture; this file is about working in the firmware code.

## Build

Cross-compiles with `arm-none-eabi-gcc` via CMake (`CMakeLists.txt`), target Cortex-M3, no OS (`CMAKE_SYSTEM_NAME Generic`).

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

Output: `inverter_firmware.elf` / `.bin`, plus a printed `arm-none-eabi-size` summary (post-build step in CMakeLists.txt). There is no unit test suite — this is embedded firmware; correctness is verified on hardware (oscilloscope on PB6/PB7) or by careful register-level reasoning.

Flashing/debugging is via OpenOCD + ST-Link, configured in `.vscode/launch.json` (cortex-debug extension), not from the command line in this repo.

## Code layout

- `include/hw_config.h` — **the single place for board-level constants**: pin assignments (`LED_PORT/PIN`, `PWM_PORT`, `PWM_PIN_CH1/CH2`) and PWM timing (`PWM_ARR`, `PWM_CCR1_FALL`, `PWM_CCR3_RISE`, `PWM_CCR4_FALL`). Change timing/pins here, not in the driver `.c` files.
- `src/clock_config.c` — HSE (external crystal) startup only. No PLL.
- `src/gpio_driver.c` — minimal bit-banged register GPIO driver (no HAL). `gpio_output_init` = 2MHz push-pull output mode; `gpio_af_output_init` = alternate-function push-pull for timer PWM.
- `src/pwm_driver.c` — TIM4 push-pull PWM: PB6 is hardware PWM (OC1), PB7 is software-toggled from `TIM4_IRQHandler` on CC3/CC4 compare match. This asymmetry (one hardware channel, one interrupt-driven) is intentional — it's how the dead-time gap between the two switching transistors is created without complementary-PWM hardware.
- `src/main.c` — wiring only: `clock_init_hse()` → `pwm_pushpull_init()` → LED GPIO init → empty loop. Keep it that thin; put logic in the driver modules.
- `include/CMSIS/`, `include/Device/` — vendor-provided (ARM CMSIS, ST device headers). Treat as read-only/generated; don't hand-edit.
- `startup/startup_stm32f103xb.s` — vendor-provided reset handler + vector table. Don't hand-edit unless adding a new interrupt handler symbol.
- `linker/STM32F103XB_FLASH.ld` — memory map for the STM32F103xB (128K flash / 20K RAM). Only touch if changing target chip or stack/heap sizing.

## Conventions to preserve

- **Direct register manipulation, no HAL/LL.** Follow the existing style (`PERIPH->REG |= FLAG;` with a comment explaining *why*, not what) rather than introducing STM32 HAL/LL calls.
- **Timing/pin constants belong in `hw_config.h`**, never hardcoded in driver source — this is the pattern the existing code already follows and should stay consistent.
- Driver modules are named `<peripheral>_driver.c/.h` (`gpio_driver`, `pwm_driver`) with an `.h` in `include/` mirroring the `.c` in `src/`. Follow this when adding a new peripheral driver (e.g. an ADC driver for the INA219/voltage-divider monitoring mentioned in the README's Phase 5 plans).
- The push-pull dead-time scheme (see README's PWM section) is safety-critical: `CCR3_RISE` must stay after `CCR1_FALL` and `CCR4_FALL` must stay before `PWM_ARR` (period wrap), with a real gap on both edges, or the two MOSFETs can be driven on simultaneously (shoot-through). Be careful with any change to these constants.

## Non-code files (context only, don't parse/edit)

- `Pushpull_ckt.asc` — LTspice schematic of the push-pull power stage this firmware drives.
- `EE20_Transformer_Spec_RevE_FINAL.docx`, `Transformer_Design_Revision_History.docx` — transformer design specs for the same stage.

These explain *why* the PWM timing constants in `hw_config.h` are what they are, but are binary/schematic formats outside normal code review.

## Scope note

This repo currently contains only the inverter (push-pull) firmware. The solar charger (CN3722 MPPT) and BMS front-end described in the README are still at the schematic/BOM planning stage and have no code yet — don't assume charger-side source files exist.
