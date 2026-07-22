# Invertor — Solar-Powered Backup System (DIY)

A personal DIY project to build a solar-powered backup power system for home use: a LiFePO4 battery charged by solar, protected by a BMS, and stepped up to 230V AC through an STM32-driven push-pull inverter. This repo contains the STM32F103 ("Blue Pill") inverter firmware plus the design documents (transformer specs, LTspice schematic) for the power stage.

---

## Project Status

> **Two-stage inverter firmware working and logic-analyzer verified (2026-07-22).** The STM32F103C8T6 runs from the external 8MHz crystal and drives: (1) an **input-side push-pull** on TIM4 — center-aligned dual-hardware-channel PWM on PB6/PB7, 25kHz, now at a fixed 16µs ON-time (the pot/ADC control has been removed); and (2) a **50Hz H-bridge output unfolder** on TIM3 (PA4–PA7). Both stages' gate timing and dead-time have been confirmed on a logic analyzer — no shoot-through on either stage (see [Verification results](#verification-results-logic-analyzer-2026-07-22)). Charger/BMS front-end is still in the design phase (see [Phases](#phases)).

---

## Bring-up Notes — Two-Stage Inverter (verified 2026-07-22)

> These notes capture the state of the firmware as flashed to the Blue Pill, and where it has been verified. Read this first if you are picking the board up for a trial.

### What changed in this revision

Three changes brought the firmware from single-stage push-pull to a verified two-stage inverter:

1. **Input side — push-pull (TIM4), reworked to center-aligned dual-hardware-channel.**
   PB6 (CH1) and PB7 (CH2) are *both* now genuine hardware PWM outputs in center-aligned mode. This replaced the earlier design where PB6 was hardware PWM but PB7 was toggled in software from `TIM4_IRQHandler` on CC3/CC4 compare matches. That software edge raced PB6's hardware-timed edge — the gap shrank to ~4µs at max duty, and interrupt latency (worst at the `-O0` debug build) could eat into it, producing real overlap/shoot-through on the scope. Center-aligned mode makes the dead-time a pure hardware property on every period, with **no ISR in the push-pull gate path**. Same 25kHz, same `(160 − duty) × 0.125µs` dead-time formula. See [PWM / Dead-time scheme](#pwm--dead-time-scheme).

2. **Output side — H-bridge unfolder (TIM3), new.**
   A 50Hz square-wave full-bridge that unfolds the transformer/rectifier HV DC rail into 230V AC. Four plain-GPIO gates on **PA4=HA, PA5=LA, PA6=HB, PA7=LB**, sequenced by a 4-phase state machine in `TIM3_IRQHandler` with an **all-off dead-time phase at every polarity swap** (1ms) so a leg is never shorted top-to-bottom. Runs independently of the push-pull (separate timer, no phase-lock). See [H-bridge output unfolder (50 Hz)](#h-bridge-output-unfolder-50-hz).

3. **Pot/ADC duty control removed — fixed operating point.**
   The push-pull now runs at a single hardcoded duty, `PWM_DUTY_FIXED_TICKS = 128` ticks = 16µs ON (the Rev E maximum), set once at startup in [main.c](src/main.c). The ADC path is gone from the active firmware (`adc_driver.c` remains in the tree but is unused and gc-sectioned out of the binary). Change the one `PWM_DUTY_FIXED_TICKS` line in [hw_config.h](include/hw_config.h) to re-tune.

Wiring: `hbridge_init()` is called from [main.c](src/main.c) after `pwm_pushpull_init()`; constants live in [hw_config.h](include/hw_config.h) §7; source added to [CMakeLists.txt](CMakeLists.txt). Flash usage is **2144 B** (of 128K). The project builds clean (`exit 0`, only the standard harmless `nosys`/RWX linker warnings).

### Verification results (logic analyzer, 2026-07-22)

Both stages were captured on a logic analyzer and cross-checked against the design. Everything matched; **no shoot-through on either stage.**

**Input side — push-pull (PB6 = Q1, PB7 = Q2):**

| Quantity | Measured | Expected | ✓ |
|---|---|---|---|
| Period / frequency | 40.00 µs → 25.00 kHz | 40 µs / 25 kHz | ✓ |
| ON-time (each channel) | 16.00 µs | 16 µs (128 ticks) | ✓ |
| Dead-time (each of 3 gaps) | 4.00 µs | 4 µs = (160−128)×0.125 | ✓ |
| Phase (Q1↔Q2 pulse centers) | 20.00 µs = 180° | 20 µs / 180° | ✓ |
| Both channels high together | never | none | ✓ |

**Output side — H-bridge (PA4=HA, PA5=LA, PA6=HB, PA7=LB):**

| Quantity | Measured | Expected | ✓ |
|---|---|---|---|
| Period / frequency | 20.00 ms → 50.0 Hz | 20 ms / 50 Hz | ✓ |
| Active conduction / half-cycle | ~9.0 ms | 9000 µs | ✓ |
| Dead-time (all-4-low) / swap | ~1.00 ms | 1000 µs | ✓ |
| Same-leg overlap (HA&LA or HB&LB) | never | none | ✓ |
| Diagonal pairs (HA+LB, then HB+LA) | correct | correct | ✓ |

> The first H-bridge capture appeared to show the wrong diagonal pairing; this was traced to the **Ch6/Ch7 logic-analyzer probes being swapped**, not a firmware or wiring fault. With the probes corrected the diagonals are exactly `HA+LB` / `HB+LA` as intended.

### ⚠️ Still to confirm before running real power

The logic-analyzer verification covers the **MCU gate signals** (timing, dead-time, no overlap). It does *not* cover the power side. Before applying HV:

1. **Gate-driver polarity.** Confirm which logic level turns each MOSFET on in your gate-driver hardware, and that it matches the HA/LA/HB/LB assignment. The H-bridge is driven as **4 independent, active-high, 3.3V-logic gate signals** (2 highs + 2 lows), dead-time enforced in firmware. If your driver board instead takes one PWM per leg (e.g. an IR2104 that generates the complement + dead-time itself), or is active-low, the scheme needs a small rework — note the part and it can be adapted.
2. **Differential output.** Scope node A vs node B across the bridge load and confirm a clean ±HV 50Hz square wave (not both nodes swinging in common mode) before connecting the transformer/load.

---

## Repository Structure

```
Invertor/
├── CMakeLists.txt                     # Cross-compile build (arm-none-eabi-gcc, Cortex-M3)
├── README.md
├── CLAUDE.md                          # Notes for AI-assisted work in this repo
├── .gitignore
├── .vscode/
│   ├── c_cpp_properties.json          # IntelliSense config (Win32 + STM32 targets)
│   └── launch.json                    # cortex-debug launch config (OpenOCD + ST-Link)
│
├── src/                                # Firmware sources
│   ├── main.c                         # Entry point: clock → push-pull PWM (fixed duty) → H-bridge → idle
│   ├── clock_config.c                 # HSE (external crystal) clock startup
│   ├── gpio_driver.c                  # Minimal register-level GPIO driver
│   ├── adc_driver.c                   # ADC1 on PA0 (pot) — retained but unused; duty is now fixed
│   ├── pwm_driver.c                   # TIM4 center-aligned push-pull PWM (dual HW channel)
│   ├── hbridge_driver.c               # TIM3 50 Hz H-bridge output unfolder + dead-time ISR
│   └── system_stm32f1xx.c             # CMSIS system init (vendor-provided)
│
├── include/                            # Headers
│   ├── hw_config.h                    # Pin mapping + PWM timing constants (board-level config)
│   ├── clock_config.h
│   ├── gpio_driver.h
│   ├── adc_driver.h
│   ├── pwm_driver.h
│   ├── hbridge_driver.h
│   ├── CMSIS/                         # ARM CMSIS core headers (Cortex-M0/M3/M4/M7/M23/M33...)
│   └── Device/                        # ST STM32F1 device headers (register definitions)
│
├── startup/
│   └── startup_stm32f103xb.s          # Reset handler + interrupt vector table (CMSIS/ST)
│
├── linker/
│   └── STM32F103XB_FLASH.ld           # Linker script: 128K FLASH / 20K RAM memory map
│
├── Pushpull_ckt.asc                   # LTspice schematic — push-pull inverter power stage
├── EE20_Transformer_Spec_RevE_FINAL.docx     # Push-pull transformer design spec
└── Transformer_Design_Revision_History.docx  # Transformer design revision notes
```

---

## System Overview

```
Solar Panel Array (12V, ~6W)
        │
        ▼
  Blocking Diode (1N5822)
        │
        ▼
  CN3722 MPPT Charge Controller
  (LiFePO4 CC/CV profile, 14.4V, 500mA)
        │
        ▼
  4S LiFePO4 BMS Module
  (Overcharge / over-discharge / short circuit protection)
        │
        ▼
  LiFePO4 Battery — 12.8V 7Ah (89.6Wh)
        │
        ▼
  Fuse (2A blade) → DC Input → STM32 Inverter (this repo)
        │
        ▼
  Push-pull (25kHz) → step-up transformer → rectifier → HV DC bus
        │
        ▼
  H-bridge unfolder (50Hz) → 230V AC Output
```

Optional monitoring layer: STM32 BluePill + INA219 (I2C) reading charge current and battery voltage.

---

## Inverter Firmware (this repo)

The firmware runs on an STM32F103C8T6 ("Blue Pill") and generates a dead-time-protected push-pull PWM drive for two MOSFETs feeding the center-tapped primary of the step-up transformer (see `Pushpull_ckt.asc` and the transformer spec `.docx` files for the power stage this drives).

### Architecture

| Module | Responsibility |
|---|---|
| `clock_config.c` | Switches system clock to the external HSE crystal (8MHz) |
| `gpio_driver.c` | Bare-metal GPIO helpers (push-pull output init, AF output init, write, toggle) |
| `adc_driver.c` | ADC1 on PA0 in continuous conversion mode — was the duty-control pot reader; **retained but no longer called** (duty is fixed) |
| `pwm_driver.c` | Configures TIM4 (center-aligned) for push-pull switching on PB6/PB7 — both native hardware channels, no ISR — and clamps duty |
| `hbridge_driver.c` | TIM3 50 Hz H-bridge output unfolder on PA4–PA7; sequences the diagonal pairs with a dead-time gap via `TIM3_IRQHandler` |
| `hw_config.h` | Single source of truth for pin mapping and PWM timing constants |
| `main.c` | Wires the above together: clock → push-pull PWM (fixed duty) → H-bridge → idle |

The codebase follows a strict layering rule: **no peripheral register access or hardware constant appears outside a driver `.c` file and `hw_config.h`.** `main.c` contains only driver calls and application logic.

### PWM / Dead-time scheme

- **Timer:** TIM4, `PSC = 0`, running in **center-aligned counting mode** (CNT sweeps `0 → ARR → 0` once per switching period). `ARR = PWM_HALF_PERIOD_TICKS = 160` → full period = `2×160 = 320` ticks @ 8MHz = 40µs = **25kHz** switching frequency. One tick = 0.125µs.
- **Channel 1 (PB6, TIM4_CH1):** Hardware PWM Mode 1 (`HIGH while CNT < CCR1`) — a pulse of width `2×CCR1`, centered on the trough (`CNT=0`, the period boundary).
- **Channel 2 (PB7, TIM4_CH2):** Hardware PWM Mode 2 (`HIGH while CNT >= CCR2`) — a pulse of width `2×(ARR-CCR2)`, centered on the peak (`CNT=ARR`), i.e. exactly half a period away from Q1.

**Both channels are genuine hardware PWM outputs — there is no ISR in the gate-drive path.** An earlier revision drove PB7 from software inside `TIM4_IRQHandler` on CC3/CC4 compare-match interrupts, racing it against PB6's hardware-timed edge. That margin shrank as duty rose (down to 4µs at max duty), and interrupt latency plus this project's `-O0` debug build could exceed it — PB7 could still be HIGH when PB6 went HIGH again, producing real overlap on the scope, worst at high duty. Moving Q2 onto its own native hardware channel (CH2) removes the race entirely: both edges are generated by compare hardware, guaranteed on every single period.

Duty is a **fixed compile-time value** (`PWM_DUTY_FIXED_TICKS = 128` ticks = 16µs ON), set once at startup — see [Fixed duty](#fixed-duty-pot-removed). Both channels are derived from the same half-width so they always get the *same* ON-time, keeping volt-seconds balanced across the transformer:

```
half_on = duty / 2
CCR1 = half_on            (Q1: width 2×CCR1, centered on trough)
CCR2 = 160 - half_on      (Q2: width 2×(160-CCR2), centered on peak)
```

Waveform at maximum duty (`duty = 128` ticks = 16µs ON, `half_on = 64`):

```
CNT:      0    64        96   160(ARR)  96   64        0
PB6 Q1:  ‾‾‾‾‾|__________________________________|‾‾‾‾‾   (mode 1, centered on trough)
PB7 Q2:  _____|____|‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾|____|_____   (mode 2, centered on peak)
              <4µs>                        <4µs>
              dead    <----- Q2 ON ----->   dead
```

**Dead-time is a derived quantity, not a fixed constant:** each gap = `(160 − duty) × 0.125µs`, symmetric on both edges — same formula as before, now enforced purely by the compare hardware rather than contingent on interrupt response time. It is *widest* at minimum duty (18µs) and *narrowest* at maximum duty (4µs), bounded by `PWM_DUTY_MAX_TICKS = 128`.

> ⚠️ **Safety-critical:** `PWM_DUTY_MAX_TICKS` must stay below `PWM_HALF_PERIOD_TICKS` (160). Raising it to 160 collapses the dead-time to zero and shoot-throughs both MOSFETs.

All timing constants live in [include/hw_config.h](include/hw_config.h) — change `PWM_HALF_PERIOD_TICKS` / `PWM_DUTY_*_TICKS` there rather than in `pwm_driver.c`.

### Fixed duty (pot removed)

The push-pull runs at a **single hardcoded ON-time**. After bench-tuning with a pot confirmed 16µs as the target, the ADC/pot control was removed in favour of a fixed operating point:

```c
/* hw_config.h */
#define PWM_DUTY_FIXED_TICKS   PWM_DUTY_MAX_TICKS   /* 128 ticks = 16 µs ON (Rev E) */
```

[main.c](src/main.c) calls `pwm_set_duty(PWM_DUTY_FIXED_TICKS)` once after `pwm_pushpull_init()`, then idles — there is no control loop. To re-tune, change that one line (clamped to `[16 .. 128]` ticks inside `pwm_set_duty()` regardless).

The ADC driver ([adc_driver.c](src/adc_driver.c)) is left in the tree for easy restoration but is no longer called, so the linker's `--gc-sections` drops it entirely from the binary. To put the pot back, restore `adc_init()`/`adc_read()` and the mapping loop from git history.

### H-bridge output unfolder (50 Hz)

Second power stage: the push-pull + transformer + rectifier produce a high-voltage DC rail; a full bridge then **unfolds** that rail into 50 Hz AC by alternating its two diagonal switch pairs. Implemented in [src/hbridge_driver.c](src/hbridge_driver.c) on **TIM3**, independent of the push-pull (separate timer, no phase-lock needed for bench work).

- **Gate pins:** PA4 = HA, PA5 = LA, PA6 = HB, PA7 = LB (Leg A high/low, Leg B high/low), plain GPIO. Change in [hw_config.h](include/hw_config.h) if your driver board wires them differently.
- **Scheme:** a 4-phase state machine advanced by the TIM3 update ISR (1µs tick):

```
phase 0  POS  : HA + LB on   9 ms   (current A → load → B)
phase 1  DEAD : all off      1 ms
phase 2  NEG  : HB + LA on   9 ms   (current B → load → A)
phase 3  DEAD : all off      1 ms
                             -----
                             20 ms  = 50 Hz
```

The **all-off DEAD phase inserted at every polarity swap** is the safety mechanism: the two switches of a leg are never commanded on in adjacent phases, so the bridge can't be shorted top-to-bottom during a transition. 1 ms is deliberately generous for a first trial — tighten `HBRIDGE_DEAD_TICKS` once the gate drive is characterised.

Unlike the 25 kHz push-pull, this stage is *not* timing-critical (50 Hz, ms-scale dead-time), so a plain GPIO + ISR state machine is used rather than hardware PWM.

The four gate signals, the 1 ms dead-time at every swap, and the `HA+LB` / `HB+LA` diagonal pairing were **confirmed on a logic analyzer (2026-07-22)** — see [Verification results](#verification-results-logic-analyzer-2026-07-22).

> ⚠️ **Before applying HV:** the logic-analyzer check covers the MCU gate signals only. Still confirm your gate-driver board's polarity (which input turns each MOSFET on) matches the HA/LA/HB/LB assignment, and scope the bridge's differential output (node A vs node B) for a clean ±HV 50Hz square wave.

### Toolchain

Development host is **Windows 11**. Versions below are what the firmware is currently built and flashed with:

| Tool | Version in use | Location / source |
|---|---|---|
| Arm GNU Toolchain (`arm-none-eabi-gcc`) | 15.2.Rel1 — gcc 15.2.1 | `C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\` |
| `arm-none-eabi-gdb` | 16.3.90 | ships with the Arm toolchain above |
| CMake | 4.3.3 | `C:\Program Files\CMake\` (`CMakeLists.txt` requires ≥ 3.20) |
| GNU Make (`mingw32-make`) | 4.2.1 | `C:\CTOOLS\mingw32\` — backs the "MinGW Makefiles" generator |
| Ninja | 1.13.2 | winget — optional alternative generator |
| OpenOCD | 0.12.0-7 (xPack) | winget (`xpack-dev-tools.openocd-xpack`) |
| Git | 2.54.0 | `C:\Program Files\Git\` |
| VS Code + `cortex-debug` | — | debug integration, see [.vscode/launch.json](.vscode/launch.json) |

Only `arm-none-eabi-gcc`, CMake, and a generator (Make or Ninja) are needed to build; OpenOCD is needed only to flash/debug. Nothing here depends on the STM32 HAL/LL libraries or STM32CubeMX — the vendor headers vendored under [include/CMSIS/](include/CMSIS/) and [include/Device/](include/Device/) are the only external code.

### Build

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..     # or "Unix Makefiles" / "Ninja" depending on your setup
cmake --build .
```

Build configuration (all in [CMakeLists.txt](CMakeLists.txt)):

- Cross-compile setup — `CMAKE_SYSTEM_NAME Generic`, `CMAKE_SYSTEM_PROCESSOR arm`, set *before* `project()`.
- `CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY` — skips CMake's hosted-environment link test, which cannot pass on bare metal.
- Compile flags: `-mcpu=cortex-m3 -mthumb -Wall -ffunction-sections -fdata-sections -O0 -g` (unoptimized + debug symbols, since bring-up is done under the debugger).
- Link flags: custom linker script, `-Wl,--gc-sections`, a `.map` file, and `--specs=nosys.specs` (no syscalls — bare metal).
- `STM32F103xB` is defined project-wide to select the right CMSIS device header.

Produces `inverter_firmware.elf` and `inverter_firmware.bin`, and prints an `arm-none-eabi-size` memory summary as a post-build step. Current usage is tiny relative to the part (128K flash / 20K RAM):

```
   text    data     bss     dec     hex  filename
   2144      20    1976    4140    102c  inverter_firmware.elf
```

There is no unit test suite — this is bare-metal firmware. Correctness is verified on hardware (logic analyzer on PB6/PB7 for the push-pull and PA4–PA7 for the H-bridge — see [Verification results](#verification-results-logic-analyzer-2026-07-22)) or by register-level reasoning.

### Flash / Debug

Debugging is configured in [.vscode/launch.json](.vscode/launch.json) via `cortex-debug` + OpenOCD, targeting an ST-Link probe (`interface/stlink.cfg`, `target/stm32f1x.cfg`), with `runToEntryPoint: main`. Launch "Debug STM32" from VS Code's Run & Debug panel with the board connected.

[.vscode/c_cpp_properties.json](.vscode/c_cpp_properties.json) carries two IntelliSense configs: **STM32** (the real one — `arm-none-eabi-gcc`, C11, `STM32F103xB`, project include paths) and a **Win32** fallback pointing at the MinGW gcc in `C:\CTOOLS\mingw32`. Select "STM32" in VS Code for correct register-definition IntelliSense.

---

## Target Specifications

| Parameter | Value |
|---|---|
| Solar input | 12V, ~500mA, ~6W |
| Solar cell configuration | 2 × (5 cells in parallel) in series |
| Each cell | 6V, 100mA, 0.5W (70×70mm polycrystalline) |
| Battery chemistry | LiFePO4 |
| Battery voltage (nominal) | 12.8V |
| Battery capacity | 7Ah (89.6Wh) |
| Charge voltage | 14.4V (CC/CV) |
| Max charge current | 500mA |
| Load | 230V AC, 10W bulb |
| Target runtime | 3 hours |
| Push-pull switching frequency | 25kHz (TIM4, center-aligned) |
| Push-pull ON-time (fixed) | 16µs per switch (128 ticks, Rev E max) |
| Push-pull dead-time | 4µs per gap (at the fixed 16µs duty) |
| H-bridge output frequency | 50Hz (TIM3 unfolder) |
| H-bridge dead-time | 1ms per polarity swap |
| Inverter efficiency (assumed) | 80% |
| Required battery energy | ~37.5Wh |

---

## Hardware Architecture

### Solar Panel Array
- 5 cells connected in parallel → 6V, 500mA
- A second identical set → 6V, 500mA
- Both sets in series → **12V, 500mA, ~6W**

### Charge Controller — CN3722 (MPPT)
- Boost converter topology (solar Voc ~12V → charge voltage 14.4V)
- Charge current set via R_prog resistor
- Charge voltage set via FB pin resistor divider
- Recommended inductor: 22µH shielded, low DCR (< 0.5Ω)
- Input cap: 10µF + 100nF ceramic (25V rated)
- Output cap: 22µF + 100nF ceramic (25V rated)
- Switching frequency: ~300kHz

**Key calculated values:**

| Component | Value | Formula |
|---|---|---|
| R_prog | ~2kΩ | I_charge = 1000 / R_prog |
| R1 (FB divider) | 110kΩ | V_charge = 1.2 × (1 + R1/R2) |
| R2 (FB divider) | 10kΩ | Target V_charge = 14.4V |
| Inductor | 22µH | L = (Vin−Vout) / (I × fsw × ΔIL) |

### Battery Protection — 4S LiFePO4 BMS
- Overcharge cutoff: **14.6V** (3.65V/cell)
- Over-discharge cutoff: **10V** (2.5V/cell)
- Short circuit and overcurrent protection (10A rated)
- ICs: DW01A (protection logic) + FS8205A (dual N-channel MOSFET switch)
- Wiring: Charger → BMS P+/P− | Battery → BMS B+/B−

### Inverter — Push-Pull Stage (STM32-driven)
- Center-tapped push-pull topology, 12.8V primary side
- Two N-channel MOSFETs switched 180° out of phase with dead-time (see [PWM / Dead-time scheme](#pwm--dead-time-scheme))
- Driven directly by STM32F103 TIM4 — PB6 (CH1) and PB7 (CH2), both hardware PWM in center-aligned mode
- Fixed 16µs ON-time (the pot/ADC control was removed after bench-tuning — see [Fixed duty](#fixed-duty-pot-removed))
- Power stage schematic: `Pushpull_ckt.asc` (LTspice)
- Transformer design: `EE20_Transformer_Spec_RevE_FINAL.docx`, `Transformer_Design_Revision_History.docx`

### STM32 Pin Map

| Pin | Function | Direction |
|---|---|---|
| PB6 | TIM4_CH1 — Q1 push-pull gate (hardware PWM, mode 1, trough-centered) | AF push-pull output |
| PB7 | TIM4_CH2 — Q2 push-pull gate (hardware PWM, mode 2, peak-centered) | AF push-pull output |
| PA0 | ADC1_IN0 — former duty-control pot (unused; duty is now fixed) | Analog input (spare) |
| PA4 | H-bridge Leg A high-side gate (HA) | GP push-pull output |
| PA5 | H-bridge Leg A low-side gate (LA) | GP push-pull output |
| PA6 | H-bridge Leg B high-side gate (HB) | GP push-pull output |
| PA7 | H-bridge Leg B low-side gate (LB) | GP push-pull output |
| PC13 | Onboard LED (defined in `hw_config.h`, not currently driven) | GP push-pull output |
| OSC_IN / OSC_OUT | 8MHz HSE crystal | — |

### Optional Monitoring — STM32 BluePill + INA219
- INA219 (I2C) in series on charge line for current measurement
- Shunt resistor: 0.1Ω, 1%, 1W
- Battery voltage divider for ADC: R1 = 100kΩ, R2 = 27kΩ
  - Maps 0–14.6V battery range to 0–3.1V at STM32 ADC pin
  - 100nF cap across R2 for noise filtering

> **Note:** this monitoring layer has no firmware yet. PA0 / ADC1 channel 0 is now free again (the duty-control pot was removed), and [adc_driver.c](src/adc_driver.c) is already written for a single conversion on that channel — so a battery-voltage read could reuse it directly. For *two* inputs (battery voltage + INA219-side current), `adc_driver.c` would need converting from its single-conversion sequence (`SQR1 L=0`) to a scan/multi-channel sequence.

### Output / Load Path
- 2A blade fuse on battery positive line
- DC input → push-pull stage → step-up transformer → rectifier → HV DC bus → H-bridge unfolder (50Hz) → 230V AC load

---

## PCB Design Plan

- **Tool:** KiCad
- **Layer count:** 2-layer (top: components + routing, bottom: solid ground plane)
- **Target fab:** JLCPCB (Gerber export)

### Layout priorities (in order)
1. Switching loop (CN3722 SW pin ↔ inductor ↔ output cap) — keep tight, minimize enclosed area
2. Ground plane on bottom layer, stitched with vias around switching section
3. Analog ground (FB divider, INA219 sense lines) star-connected to power ground at CN3722 GND pin
4. High-current traces (battery, fuse, charge output): ≥ 1.5mm wide at 1oz copper
5. STM32 and I2C traces routed away from the switching section

### Package choices (for hand soldering)
| IC | Package |
|---|---|
| CN3722 | SOP-8 (preferred over QFN for hand soldering) |
| INA219 | SOT-23-8 |
| MOSFETs (load switch) | SOT-23 or D-PAK (TO-252) |
| Passives | 0805 minimum |

---

## Datasheets to Study (Phase 1 reading list)

- [ ] CN3722 — MPPT solar charger IC (Consonance)
- [ ] DW01A — Single-cell Li battery protection IC
- [ ] FS8205A — Dual N-channel MOSFET for BMS switch

---

## Bill of Materials (Draft)

| Component | Value / Part | Source |
|---|---|---|
| Solar cells | 70×70mm, 6V 100mA polycrystalline (×10) | Amazon India |
| Battery | LiFePO4 12.8V 7Ah | Amazon India / Keltron |
| Charge controller IC | CN3722 (SOP-8) | Robu.in / Lamington Road |
| BMS module or ICs | 4S LiFePO4 BMS (DW01A + FS8205A) | Lamington Road |
| Blocking diode | 1N5822 Schottky | Lamington Road |
| TVS diode | P6KE15A | Lamington Road |
| Inductor | 22µH shielded, DCR < 0.5Ω (CDRH6D28 or similar) | Lamington Road |
| Input capacitor | 10µF ceramic, 25V, X5R/X7R | Lamington Road |
| Output capacitor | 22µF ceramic, 25V, X5R/X7R | Lamington Road |
| R_prog | 2kΩ, 1%, 0805 | Lamington Road |
| FB divider R1 | 110kΩ, 1%, 0805 | Lamington Road |
| FB divider R2 | 10kΩ, 1%, 0805 | Lamington Road |
| Voltage divider R1 | 100kΩ, 1%, 0805 | Lamington Road |
| Voltage divider R2 | 27kΩ, 1%, 0805 | Lamington Road |
| INA219 | SOT-23-8 or module | Amazon India |
| Shunt resistor | 0.1Ω, 1%, 1W | Lamington Road |
| Fuse + holder | 2A blade automotive | Local / Lamington Road |
| Duty-control pot | 200Ω trimmer/rotary — *bench-tuning only; not needed now duty is fixed* | Lamington Road |
| STM32 BluePill | STM32F103C8T6 | Amazon India / Robu.in |
| ST-Link V2 programmer | ST-Link V2 clone or genuine | Amazon India / Robu.in |

---

## Phases

- [x] **Phase 0** — Concept and load/battery sizing
- [ ] **Phase 1** — Datasheet study, component value calculations, schematic (KiCad)
- [ ] **Phase 2** — PCB layout (KiCad), Gerber generation, send to JLCPCB
- [ ] **Phase 3** — Assembly and bring-up
- [x] **Phase 4 (started early)** — Two-stage inverter firmware working and logic-analyzer verified on STM32 Blue Pill: center-aligned dual-hardware-channel push-pull (TIM4, 25kHz, fixed 16µs duty, 4µs dead-time) + 50Hz H-bridge unfolder (TIM3, PA4–PA7, 1ms dead-time), HSE crystal clock, no shoot-through on either stage; transformer spec and LTspice schematic drafted
- [ ] **Phase 5** — WiFi control + LDR automation (STM32 + ESP8266/ESP32)

---

## Tools Used

**Firmware** (versions in [Toolchain](#toolchain)):

- Arm GNU Toolchain (`arm-none-eabi-gcc` 15.2.Rel1) — cross-compiler, assembler, linker, `objcopy`/`size`
- CMake 4.3.3 + MinGW Make 4.2.1 (or Ninja 1.13.2) — build system
- OpenOCD 0.12.0 (xPack) + ST-Link — flashing and on-chip debugging
- `arm-none-eabi-gdb` 16.3.90 — driven through cortex-debug, not usually invoked by hand
- VS Code + cortex-debug extension — editor, IntelliSense, debug front-end
- Git 2.54.0 — version control

**Hardware / design:**

- KiCad — schematic and PCB layout (Phases 1–2, not yet started)
- LTspice — push-pull power stage simulation (`Pushpull_ckt.asc`)
- Logic analyzer — verified both stages' gate timing and dead-time (push-pull PB6/PB7, H-bridge PA4–PA7); confirmed no shoot-through (2026-07-22)
- Oscilloscope + DMM — bench bring-up

**Not used, deliberately:** STM32 HAL/LL, STM32CubeMX/CubeIDE, or any RTOS. Everything is register-level C against the CMSIS + ST device headers vendored in [include/](include/).

---

## Notes

- Component sourcing: Lamington Road, Mumbai for most passives and ICs; Amazon India for modules and the battery.
- The push-pull inverter firmware (this repo's `src/`, `include/`, `startup/`, `linker/`) is being prototyped ahead of the charger/BMS PCB (Phases 1–3) so the transformer and switching scheme can be validated on the bench first.
- PCB bring-up will require a bench power supply and DMM at minimum; oscilloscope strongly recommended for verifying CN3722 switching waveform and the TIM4 push-pull dead-time edges.


## Changes required

Need a typedef file which will have all uint16_t names defined properly as it looks messy in code.
