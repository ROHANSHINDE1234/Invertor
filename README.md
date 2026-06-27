# Solar-Powered Backup System — DIY Project

A personal DIY project to build a solar-powered backup system for home use, built around a LiFePO4 battery, a custom PCB charger, and an STM32 BluePill microcontroller.

---

## Project Status

> **Phase 1 in progress — System design and component selection**

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
  Fuse (2A blade) → DC Output → Inverter (Phase 2)
```

Optional monitoring layer: STM32 BluePill + INA219 (I2C) reading charge current and battery voltage.

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

### Optional Monitoring — STM32 BluePill + INA219
- INA219 (I2C) in series on charge line for current measurement
- Shunt resistor: 0.1Ω, 1%, 1W
- Battery voltage divider for ADC: R1 = 100kΩ, R2 = 27kΩ
  - Maps 0–14.6V battery range to 0–3.1V at STM32 ADC pin
  - 100nF cap across R2 for noise filtering

### Output / Load Path
- 2A blade fuse on battery positive line
- DC output to inverter (inverter design is Phase 2 / out of scope for this phase)

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
- [ ] **Phase 4** — Inverter design and AC load integration (230V, 10W)
- [ ] **Phase 5** — WiFi control + LDR automation (STM32 + ESP8266/ESP32)

---

## Tools Used

- KiCad — Schematic and PCB layout
- VS Code — Firmware development
- Git — Version control
- STM32CubeIDE / PlatformIO — STM32 firmware

---

## Notes

- Component sourcing: Lamington Road, Mumbai for most passives and ICs; Amazon India for modules and the battery.
- Inverter design (H-bridge, SPWM, gate drivers IR2110/IR2104, step-up transformer) is tracked separately and begins after Phase 3.
- PCB bring-up will require a bench power supply and DMM at minimum; oscilloscope strongly recommended for verifying CN3722 switching waveform.
