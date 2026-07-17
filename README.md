# Invertor — Solar-Powered Backup System (DIY)

A personal DIY project to build a solar-powered backup power system for home use: a LiFePO4 battery charged by solar, protected by a BMS, and stepped up to 230V AC through an STM32-driven push-pull inverter. This repo contains the STM32F103 ("Blue Pill") inverter firmware plus the design documents (transformer specs, LTspice schematic) for the power stage.

---

## Project Status

> **Inverter Stage 1 firmware working** — TIM4-based push-pull PWM generation on the STM32F103C8T6, running from the external 8MHz crystal, with duty cycle live-adjustable from a potentiometer on PA0 (ADC1). Dead-time is enforced in firmware and was validated against a logic analyzer capture. Charger/BMS front-end is still in the design phase (see [Phases](#phases)).

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
│   ├── main.c                         # Entry point: clock → ADC → PWM init, pot control loop
│   ├── clock_config.c                 # HSE (external crystal) clock startup
│   ├── gpio_driver.c                  # Minimal register-level GPIO driver
│   ├── adc_driver.c                   # ADC1 continuous conversion on PA0 (pot input)
│   ├── pwm_driver.c                   # TIM4 push-pull PWM + dead-time ISR
│   └── system_stm32f1xx.c             # CMSIS system init (vendor-provided)
│
├── include/                            # Headers
│   ├── hw_config.h                    # Pin mapping + PWM timing constants (board-level config)
│   ├── clock_config.h
│   ├── gpio_driver.h
│   ├── adc_driver.h
│   ├── pwm_driver.h
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
  Fuse (2A blade) → DC Input → STM32 Push-Pull Inverter (this repo)
        │
        ▼
  Push-pull transformer → 230V AC Output
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
| `adc_driver.c` | ADC1 on PA0 in continuous conversion mode — reads the duty-control pot |
| `pwm_driver.c` | Configures TIM4 for push-pull switching, handles dead-time via `TIM4_IRQHandler`, clamps duty |
| `hw_config.h` | Single source of truth for pin mapping and PWM timing constants |
| `main.c` | Wires the above together: clock → ADC → PWM → pot-to-duty control loop |

The codebase follows a strict layering rule: **no peripheral register access or hardware constant appears outside a driver `.c` file and `hw_config.h`.** `main.c` contains only driver calls and application logic.

### PWM / Dead-time scheme

- **Timer:** TIM4, `PSC = 0`, `ARR = 319` → 320 ticks/period @ 8MHz = 40µs period = **25kHz** switching frequency. One tick = 0.125µs.
- **Channel 1 (PB6, TIM4_CH1):** Hardware PWM Mode 1 — high from the period boundary, falls at `CCR1`. Zero software involvement, zero jitter.
- **Channel 2 (PB7):** Plain GPIO, software-driven from `TIM4_IRQHandler` on CC3/CC4 compare matches. `CCR3` (rise) is fixed at tick 160; `CCR4` (fall) tracks the duty.

Duty is **live-controlled by the potentiometer on PA0**. Both channels always get the *same* ON-time, which keeps volt-seconds balanced across the transformer:

```
CCR1 = duty                       (Q1 falling edge)
CCR3 = 160  ← fixed, never changes (Q2 rising edge, always t=20µs)
CCR4 = 160 + duty                 (Q2 falling edge)
```

Waveform at maximum duty (`duty = 128` ticks = 16µs ON):

```
t (µs):  0    16   20   36   40
         |    |    |    |    |
PB6 Q1:  ‾‾‾‾‾_______________‾‾‾    (hardware PWM, zero jitter)
PB7 Q2:  __________‾‾‾‾‾_______     (ISR-driven, 20µs fixed start)
              <-4->     <-4->
              dead      dead
```

**Dead-time is a derived quantity, not a fixed constant:** each gap = `(160 − duty) × 0.125µs`, symmetric on both edges. It is *widest* at minimum duty (18µs) and *narrowest* at maximum duty (4µs), so the worst case is bounded by `PWM_DUTY_MAX_TICKS = 128`. `pwm_set_duty()` clamps to `[16 .. 128]` ticks (`[2 .. 16µs]` ON) — this clamp is what guarantees the gap can never close, and `CCR4_max = 288 < 320` guarantees CC4 always fires inside the period.

> ⚠️ **Safety-critical:** `PWM_DUTY_MAX_TICKS` must stay below `PWM_HALF_PERIOD_TICKS` (160). Raising it to 160 collapses the dead-time to zero and shoot-throughs both MOSFETs.

**`CCR4` preload (`OC4PE`) is load-bearing, not an optimization.** Without it, a `CCR4` write lands immediately; if the counter has already passed the new value, the CC4 interrupt is silently skipped for that period, PB7 stays HIGH into the next period and overlaps PB6. This was observed as real shoot-through on a logic analyzer capture (CC4 firing at tick 8 instead of 256, ~1µs overlap every period) before `OC4PE` was enabled. `CCR1` uses `OC1PE` and `ARR` uses `ARPE` for the same buffering reason.

All timing constants live in [include/hw_config.h](include/hw_config.h) — change `PWM_ARR` / `PWM_DUTY_*_TICKS` / `PWM_CCR3_RISE` there rather than in `pwm_driver.c`.

### Pot duty control

A 200Ω potentiometer (wiper → PA0, ends across 3.3V/GND) sets the duty at runtime:

| Stage | Detail |
|---|---|
| ADC | ADC1 channel 0, continuous conversion mode (a fresh result is always ready) |
| ADC clock | PCLK2 / 6 = 8MHz / 6 ≈ 1.33MHz (STM32F103 limit: 14MHz) |
| Sampling time | 239.5 cycles ≈ 180µs/conversion — accuracy over speed, fine for a hand-turned knob |
| Calibration | `RSTCAL` then `CAL` run at init; required for rated accuracy on STM32F1 |
| Mapping | `duty = 16 + (adc × 112) / 4095` → `[16 .. 128]` ticks, re-clamped inside `pwm_set_duty()` |

Pot fully CCW → 0 → 2µs ON (minimum power). Pot fully CW → 4095 → 16µs ON (maximum per the transformer Rev E spec).

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
   2128      20    1976    4124    101c  inverter_firmware.elf
```

There is no unit test suite — this is bare-metal firmware. Correctness is verified on hardware (oscilloscope / logic analyzer on PB6 and PB7) or by register-level reasoning.

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
| Inverter switching frequency | 25kHz (TIM4 push-pull) |
| Inverter ON-time range | 2µs – 16µs per switch, pot-adjustable (16–128 ticks) |
| Inverter dead-time | 4µs – 18µs per gap (widens as duty drops) |
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
- Driven directly by STM32F103 GPIO/TIM4 (PB6 hardware PWM, PB7 software-timed)
- 200Ω duty-control pot on PA0 (ADC1 CH0) for bench adjustment of ON-time
- Power stage schematic: `Pushpull_ckt.asc` (LTspice)
- Transformer design: `EE20_Transformer_Spec_RevE_FINAL.docx`, `Transformer_Design_Revision_History.docx`

### STM32 Pin Map

| Pin | Function | Direction |
|---|---|---|
| PB6 | TIM4_CH1 — Q1 gate drive (hardware PWM) | AF push-pull output |
| PB7 | Q2 gate drive (ISR-driven from TIM4 CC3/CC4) | GP push-pull output |
| PA0 | ADC1_IN0 — duty-control pot wiper | Analog input |
| PC13 | Onboard LED (defined in `hw_config.h`, not currently driven) | GP push-pull output |
| OSC_IN / OSC_OUT | 8MHz HSE crystal | — |

### Optional Monitoring — STM32 BluePill + INA219
- INA219 (I2C) in series on charge line for current measurement
- Shunt resistor: 0.1Ω, 1%, 1W
- Battery voltage divider for ADC: R1 = 100kΩ, R2 = 27kΩ
  - Maps 0–14.6V battery range to 0–3.1V at STM32 ADC pin
  - 100nF cap across R2 for noise filtering

> **Note:** this monitoring layer has no firmware yet. When it is added, the battery divider must land on an ADC channel *other than* channel 0 — PA0 is already taken by the duty-control pot, and `adc_driver.c` currently configures a single-conversion sequence (`SQR1 L=0`). Adding a second input means converting it to a scan/multi-channel sequence rather than just changing the pin.

### Output / Load Path
- 2A blade fuse on battery positive line
- DC output → push-pull inverter stage → 230V AC load

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
| Duty-control pot | 200Ω trimmer/rotary (PWM ON-time adjust, PA0) | Lamington Road |
| STM32 BluePill | STM32F103C8T6 | Amazon India / Robu.in |
| ST-Link V2 programmer | ST-Link V2 clone or genuine | Amazon India / Robu.in |

---

## Phases

- [x] **Phase 0** — Concept and load/battery sizing
- [ ] **Phase 1** — Datasheet study, component value calculations, schematic (KiCad)
- [ ] **Phase 2** — PCB layout (KiCad), Gerber generation, send to JLCPCB
- [ ] **Phase 3** — Assembly and bring-up
- [x] **Phase 4 (started early)** — Inverter push-pull firmware working on STM32 Blue Pill: TIM4 PWM + dead-time, HSE crystal clock, pot-controlled duty via ADC1, shoot-through bug fixed (`CCR4` preload) and verified on a logic analyzer; transformer spec and LTspice schematic drafted
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
- Logic analyzer — verified the PB6/PB7 dead-time gap and caught the `CCR4` preload shoot-through bug
- Oscilloscope + DMM — bench bring-up

**Not used, deliberately:** STM32 HAL/LL, STM32CubeMX/CubeIDE, or any RTOS. Everything is register-level C against the CMSIS + ST device headers vendored in [include/](include/).

---

## Notes

- Component sourcing: Lamington Road, Mumbai for most passives and ICs; Amazon India for modules and the battery.
- The push-pull inverter firmware (this repo's `src/`, `include/`, `startup/`, `linker/`) is being prototyped ahead of the charger/BMS PCB (Phases 1–3) so the transformer and switching scheme can be validated on the bench first.
- PCB bring-up will require a bench power supply and DMM at minimum; oscilloscope strongly recommended for verifying CN3722 switching waveform and the TIM4 push-pull dead-time edges.


## Changes required

Need a typedef file which will have all uint16_t names defined properly as it looks messy in code.
