# Invertor — Solar-Powered Backup System (DIY)

A personal DIY project to build a solar-powered backup power system for home use: a LiFePO4 battery charged by solar, protected by a BMS, and stepped up to 230V AC through an STM32-driven push-pull inverter. This repo contains the STM32F103 ("Blue Pill") inverter firmware plus the design documents (transformer specs, LTspice schematic) for the power stage.

---

## Project Status

> **Inverter Stage 1 firmware working** — TIM4-based push-pull PWM generation on the STM32F103C8T6, running from the external 8MHz crystal. Charger/BMS front-end is still in the design phase (see [Phases](#phases)).

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
│   ├── main.c                         # Entry point: clock init, PWM init, main loop
│   ├── clock_config.c                 # HSE (external crystal) clock startup
│   ├── gpio_driver.c                  # Minimal register-level GPIO driver
│   ├── pwm_driver.c                   # TIM4 push-pull PWM + dead-time ISR
│   └── system_stm32f1xx.c             # CMSIS system init (vendor-provided)
│
├── include/                            # Headers
│   ├── hw_config.h                    # Pin mapping + PWM timing constants (board-level config)
│   ├── clock_config.h
│   ├── gpio_driver.h
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
| `pwm_driver.c` | Configures TIM4 for push-pull switching and handles dead-time via `TIM4_IRQHandler` |
| `hw_config.h` | Single source of truth for pin mapping and PWM timing constants |
| `main.c` | Wires the above together: clock → PWM → status LED → idle loop |

### PWM / Dead-time scheme

- **Timer:** TIM4, `PSC = 0`, `ARR = 319` → 320 ticks/period @ 8MHz = 40µs period = **25kHz** switching frequency.
- **Channel 1 (PB6, TIM4_CH1):** Hardware PWM output — high from period start, falls at `CCR1` (dead-time edge for Q1).
- **Channel 2 (PB7):** Plain GPIO, software-driven from `TIM4_IRQHandler` using CC3/CC4 compare interrupts (`CCR3` = rise, `CCR4` = fall), so Q1 and Q2 never overlap.

```
t=0µs    Q1 rises (hardware, period boundary)
t=12µs   Q1 falls  (CCR1=60)
t=20µs   Q2 rises  (CCR3=160, ISR)   ← 10µs dead-time gap
t=32µs   Q2 falls  (CCR4=220, ISR)
t=40µs   wraps     ← 10µs dead-time gap
```

All timing constants live in [include/hw_config.h](include/hw_config.h) — change `PWM_ARR`/`PWM_CCR*` there rather than in `pwm_driver.c`.

### Build

Requires the `arm-none-eabi` GCC toolchain and CMake ≥ 3.20.

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..     # or "Unix Makefiles" / Ninja depending on your setup
cmake --build .
```

Produces `inverter_firmware.elf` and `inverter_firmware.bin`, and prints a memory usage summary (`arm-none-eabi-size`) as a post-build step.

### Flash / Debug

Debugging is configured in [.vscode/launch.json](.vscode/launch.json) via `cortex-debug` + OpenOCD, targeting an ST-Link probe (`interface/stlink.cfg`, `target/stm32f1x.cfg`). Launch "Debug STM32" from VS Code's Run & Debug panel with the target board connected.

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
- Power stage schematic: `Pushpull_ckt.asc` (LTspice)
- Transformer design: `EE20_Transformer_Spec_RevE_FINAL.docx`, `Transformer_Design_Revision_History.docx`

### Optional Monitoring — STM32 BluePill + INA219
- INA219 (I2C) in series on charge line for current measurement
- Shunt resistor: 0.1Ω, 1%, 1W
- Battery voltage divider for ADC: R1 = 100kΩ, R2 = 27kΩ
  - Maps 0–14.6V battery range to 0–3.1V at STM32 ADC pin
  - 100nF cap across R2 for noise filtering

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
| STM32 BluePill | STM32F103C8T6 | Amazon India / Robu.in |

---

## Phases

- [x] **Phase 0** — Concept and load/battery sizing
- [ ] **Phase 1** — Datasheet study, component value calculations, schematic (KiCad)
- [ ] **Phase 2** — PCB layout (KiCad), Gerber generation, send to JLCPCB
- [ ] **Phase 3** — Assembly and bring-up
- [x] **Phase 4 (started early)** — Inverter push-pull firmware (TIM4 PWM + dead-time) working on STM32 Blue Pill; transformer spec and LTspice schematic drafted
- [ ] **Phase 5** — WiFi control + LDR automation (STM32 + ESP8266/ESP32)

---

## Tools Used

- KiCad — Schematic and PCB layout
- LTspice — Push-pull power stage simulation (`Pushpull_ckt.asc`)
- VS Code + cortex-debug — Firmware development and hardware debugging
- CMake + arm-none-eabi-gcc — Firmware build toolchain
- OpenOCD + ST-Link — Flashing/debugging the STM32F103
- Git — Version control

---

## Notes

- Component sourcing: Lamington Road, Mumbai for most passives and ICs; Amazon India for modules and the battery.
- The push-pull inverter firmware (this repo's `src/`, `include/`, `startup/`, `linker/`) is being prototyped ahead of the charger/BMS PCB (Phases 1–3) so the transformer and switching scheme can be validated on the bench first.
- PCB bring-up will require a bench power supply and DMM at minimum; oscilloscope strongly recommended for verifying CN3722 switching waveform and the TIM4 push-pull dead-time edges.
