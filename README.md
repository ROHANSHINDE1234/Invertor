# Invertor — Solar-Powered Backup System (DIY)

A personal DIY project to build a solar-powered backup power system for home use: a LiFePO4 battery charged by solar, protected by a BMS, and stepped up to 230V AC through an STM32-driven two-stage inverter. This repo holds the STM32F103 ("Blue Pill") firmware plus the design documents (transformer specs, LTspice schematic) for the power stage.

---

## Contents

- [Project Status](#project-status)
- [System Overview](#system-overview)
- [Repository Structure](#repository-structure)
- [Firmware](#firmware)
  - [Architecture](#architecture)
  - [Start-up sequence](#start-up-sequence)
  - [Push-pull PWM & dead-time](#push-pull-pwm--dead-time)
  - [H-bridge unfolder (50 Hz)](#h-bridge-unfolder-50-hz)
  - [Gate-drive polarity (active-low for 6N137 → IR2110)](#gate-drive-polarity-active-low-for-6n137--ir2110)
  - [Logic-analyzer verification](#logic-analyzer-verification)
- [Hardware Bring-up Trial Log (Stage 1 push-pull)](#hardware-bring-up-trial-log-stage-1-push-pull)
- [Before Applying High Voltage](#before-applying-high-voltage)
- [Hardware Architecture](#hardware-architecture)
- [Target Specifications](#target-specifications)
- [Build, Flash & Toolchain](#build-flash--toolchain)
- [PCB Design Plan](#pcb-design-plan)
- [Bill of Materials](#bill-of-materials)
- [Phases](#phases)
- [Tools Used](#tools-used)
- [Notes](#notes)

---

## Project Status

**Firmware — working, logic-analyzer verified (2026-11-16).** Two stages on an STM32F103C8T6 running from an 8 MHz HSE crystal:

- **Stage 1 — push-pull (TIM4):** center-aligned dual-hardware-channel PWM on PB6/PB7, 25 kHz, fixed 16 µs ON-time reached via a ~0.5 s soft-start ramp.
- **Stage 2 — H-bridge unfolder (TIM3):** 50 Hz square-wave on PA4–PA7, gated by an active-high enable (PA1 → both IR2110 `SD` pins) after a 5 ms bootstrap precharge.

Gate drive is **active-low** to compensate the inverting 6N137 optocouplers. Power-up is glitch-free (all gates forced OFF before the clock switch). Both stages' timing, dead-time and start-up sequence are confirmed on a logic analyzer — no shoot-through on either stage.

**Hardware — Stage 1 validated (2026-11-12).** The push-pull power stage has been bench-validated on a soldered general-purpose board: **12 V → ~205 V DC at ~9.7 W, ~78 % efficient** at full load, switches and transformer cool (see the [Trial Log](#hardware-bring-up-trial-log-stage-1-push-pull)).

**Next:** Stage 2 (H-bridge) hardware bring-up. The charger/BMS front-end is still at the design stage (see [Phases](#phases)).

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
├── src/                               # Firmware sources
│   ├── main.c                         # Entry: safe states → clock → H-bridge → push-pull → soft-start → idle
│   ├── clock_config.c                 # HSE (external crystal) clock startup
│   ├── gpio_driver.c                  # Minimal register-level GPIO driver
│   ├── adc_driver.c                   # ADC1 on PA0 (pot) — retained but unused; duty is now fixed
│   ├── pwm_driver.c                   # TIM4 center-aligned push-pull PWM (dual HW channel)
│   ├── hbridge_driver.c               # TIM3 50 Hz H-bridge unfolder + enable + dead-time ISR
│   └── system_stm32f1xx.c             # CMSIS system init (vendor-provided)
│
├── include/                           # Headers
│   ├── hw_config.h                    # Single source of truth: pins, timing, gate polarity
│   ├── clock_config.h
│   ├── gpio_driver.h
│   ├── adc_driver.h
│   ├── pwm_driver.h
│   ├── hbridge_driver.h
│   ├── CMSIS/                         # ARM CMSIS core headers
│   └── Device/                        # ST STM32F1 device headers (register definitions)
│
├── startup/
│   └── startup_stm32f103xb.s          # Reset handler + interrupt vector table (CMSIS/ST)
│
├── linker/
│   └── STM32F103XB_FLASH.ld           # Linker script: 128K FLASH / 20K RAM
│
├── Pushpull_ckt.asc                   # LTspice schematic — push-pull power stage
├── EE20_Transformer_Spec_RevE_FINAL.docx     # Push-pull transformer design spec
└── Transformer_Design_Revision_History.docx  # Transformer design revision notes
```

---

## Firmware

Bare-metal register-level C (CMSIS core + ST device headers, no HAL/LL). Builds to **2508 B** flash of the 128K part. There is no unit-test suite — correctness is verified on hardware (logic analyzer) or by register-level reasoning.

### Architecture

| Module | Responsibility |
|---|---|
| `clock_config.c` | Switches the system clock to the external HSE crystal (8 MHz, no PLL) |
| `gpio_driver.c` | Bare-metal GPIO helpers — output init, AF output init, write, toggle, plus `gpio_output_init_level` (glitch-free level-before-output) |
| `adc_driver.c` | ADC1 on PA0 — was the duty-control pot reader; **retained but no longer called** (duty is fixed), gc-sectioned out of the binary |
| `pwm_driver.c` | TIM4 center-aligned push-pull on PB6/PB7 (both native hardware channels, no ISR); `pwm_safe_state`, `pwm_soft_start` |
| `hbridge_driver.c` | TIM3 50 Hz H-bridge unfolder on PA4–PA7 + PA1 enable; bootstrap precharge, diagonal sequencing via `TIM3_IRQHandler`, `hbridge_safe_state`, `hbridge_disable` |
| `hw_config.h` | Single source of truth for pins, PWM/H-bridge timing, gate polarity, enable/precharge/soft-start constants |
| `main.c` | Flow only: safe states → clock → H-bridge → push-pull → soft-start → idle |

**Layering rule:** no peripheral register access or hardware constant appears outside a driver `.c` file and `hw_config.h`; `main.c` contains only driver calls.

### Start-up sequence

`main.c` brings the board up in a controlled, gate-safe order:

```
hbridge_safe_state()  →  pwm_safe_state()  →  clock_init_hse()
     →  hbridge_init()  →  pwm_pushpull_init()  →  pwm_soft_start()  →  idle
```

1. **Glitch-free safe state (before the clock switch).** `hbridge_safe_state()` and `pwm_safe_state()` run first, as plain GPIO, driving all six gates to their OFF level and the H-bridge enable (PA1) to disabled. The helper `gpio_output_init_level()` latches each pin's level *while it is still an input*, then switches it to output, so the pin comes up directly at OFF with **no transient LOW**. This matters because the Stage-1 IR2110 has `SD` tied low (always enabled), so PB6/PB7 must read OFF from the first instant.
2. **H-bridge precharge + enable.** `hbridge_init()` holds **both low-side gates (LA + LB) on for 5 ms** to charge each IR2110 high-side bootstrap cap, then asserts the enable (PA1) **last**. The first TIM3 update ends the precharge and the normal unfold begins.
3. **Push-pull soft-start.** `pwm_soft_start()` ramps the duty from minimum to `PWM_DUTY_FIXED_TICKS` over **~0.5 s** (ON-time widening 2 µs → 16 µs), polling TIM4's 20 µs update flag between 1-tick increments, instead of a hard step.

### Push-pull PWM & dead-time

- **Timer:** TIM4, `PSC = 0`, **center-aligned** (CNT sweeps `0 → ARR → 0` once per switching period). `ARR = PWM_HALF_PERIOD_TICKS = 160` → full period `2×160 = 320` ticks @ 8 MHz = 40 µs = **25 kHz**. One tick = 0.125 µs.
- **PB6 (Q1, TIM4_CH1):** PWM Mode 1 (`HIGH while CNT < CCR1`) — pulse of width `2×CCR1`, centered on the trough (`CNT=0`).
- **PB7 (Q2, TIM4_CH2):** PWM Mode 2 (`HIGH while CNT >= CCR2`) — pulse of width `2×(ARR-CCR2)`, centered on the peak (`CNT=ARR`), half a period from Q1.

Both channels are genuine hardware PWM outputs — **no ISR in the gate path**. (An earlier revision toggled PB7 in software from `TIM4_IRQHandler`, racing PB6's hardware edge; the margin shrank to 4 µs at full duty and interrupt latency could exceed it, producing real overlap on the scope. Center-aligned dual hardware channels removed the race entirely.)

Both channels derive from the same half-width, so Q1 and Q2 always get the *same* ON-time (volt-second balance):

```
half_on = duty / 2
CCR1 = half_on            (Q1: width 2×CCR1, centered on trough)
CCR2 = 160 - half_on      (Q2: width 2×(160-CCR2), centered on peak)
```

Waveform at full duty (`duty = 128` ticks = 16 µs ON, `half_on = 64`):

```
CNT:      0    64        96   160(ARR)  96   64        0
PB6 Q1:  ‾‾‾‾‾|__________________________________|‾‾‾‾‾   (mode 1, trough)
PB7 Q2:  _____|____|‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾|____|_____   (mode 2, peak)
              <4µs>                        <4µs>
              dead    <----- Q2 ON ----->   dead
```

**Dead-time is derived, not fixed:** each gap = `(160 − duty) × 0.125 µs`, symmetric on both edges, enforced purely by the compare hardware. It is widest at minimum duty (18 µs) and narrowest at full duty (4 µs).

> ⚠️ **Safety-critical:** `PWM_DUTY_MAX_TICKS` (128) must stay below `PWM_HALF_PERIOD_TICKS` (160). At 160 the dead-time collapses to zero and both MOSFETs' pulses overlap (shoot-through).

**Fixed duty (pot removed).** After bench-tuning confirmed 16 µs as the target, the ADC/pot control was dropped for a fixed operating point:

```c
/* hw_config.h */
#define PWM_DUTY_FIXED_TICKS   PWM_DUTY_MAX_TICKS   /* 128 ticks = 16 µs ON (Rev E) */
```

`main.c` calls `pwm_soft_start(PWM_DUTY_FIXED_TICKS)` once (ramping to this value), then idles — no control loop. Change that one line to re-tune (clamped to `[16 .. 128]` ticks inside `pwm_set_duty()`). `adc_driver.c` is left in the tree for easy restoration but is gc-sectioned out.

### H-bridge unfolder (50 Hz)

The push-pull + transformer + rectifier produce a high-voltage DC rail; this full bridge **unfolds** it into 50 Hz AC by alternating diagonal switch pairs. Implemented on **TIM3**, independent of the push-pull (separate timer, no phase-lock). Gate pins: **PA4 = HA, PA5 = LA, PA6 = HB, PA7 = LB**.

A 4-phase state machine, advanced by the TIM3 update ISR (1 µs tick):

```
phase 0  POS  : HA + LB on   9 ms   (current A → load → B)
phase 1  DEAD : all off      1 ms
phase 2  NEG  : HB + LA on   9 ms   (current B → load → A)
phase 3  DEAD : all off      1 ms
                             -----
                             20 ms  = 50 Hz
```

The **all-off DEAD phase at every polarity swap** is the safety mechanism: the two switches of a leg are never commanded on in adjacent phases, so the bridge can't be shorted top-to-bottom during a transition. (1 ms is generous for a first trial — tighten `HBRIDGE_DEAD_TICKS` later.) Unlike the 25 kHz push-pull, this stage is not timing-critical, so a plain GPIO + ISR state machine is used.

**Enable + precharge (PA1 → IR2110 `SD`).** `hbridge_init()` does not jump straight into the unfold: it first holds LA + LB on for 5 ms (`HBRIDGE_PRECHARGE_TICKS`) to charge the IR2110 bootstrap caps, then asserts PA1 (active-high → both `SD` pins via a dedicated 6N137) **last**. PA1 HIGH enables the bridge; LOW/floating disables it. `hbridge_disable()` drops PA1 and all gates for fault handling.

### Gate-drive polarity (active-low for 6N137 → IR2110)

All gate signals run `MCU → 6N137 (inverts) → IR2110 (non-inverting) → MOSFET` — exactly **one** net inversion. The firmware cancels it by driving the gates **active-low** (`GATE_DRIVE_INVERTED = 1`, `hw_config.h` §8):

- **Push-pull:** TIM4 `CC1P`/`CC2P` output-polarity bits (timing / dead-time width unchanged).
- **H-bridge:** `GATE_ACTIVE` / `GATE_IDLE` levels.

Result: gate-on = MCU LOW; the all-off dead-time (MCU all HIGH → opto all LOW → IR2110 all LOW) = all MOSFETs OFF. Correct as flashed for 6N137 + IR2110. (If the driver is ever swapped for an *inverting* one, set `GATE_DRIVE_INVERTED = 0`, or the dead-time becomes all-on.) The PA1 enable is the exception — it is fixed active-high and does **not** go through the `GATE_*` macros. The physical wiring is in [Gate-drive isolation](#gate-drive-isolation).

### Logic-analyzer verification

Both stages captured and cross-checked against the design, on the active-low build. A gate is ON when its MCU pin is **LOW**; dead-time reads as the relevant pins **HIGH**. Steady-state numbers below re-confirmed from `updated_timings.csv` (2026-11-16; 53.6 ms span, 1341 push-pull periods, 2.7 H-bridge periods), with **PA1 (enable) HIGH** throughout.

**Push-pull (PB6 = Q1, PB7 = Q2):** ✅

| Quantity | Measured | Expected | ✓ |
|---|---|---|---|
| Period / frequency | 40.0 µs → 25.0 kHz | 40 µs / 25 kHz | ✓ |
| ON-time each (LOW pulse) | 16.0 µs | 16 µs (128 ticks) | ✓ |
| Dead-time each gap (both HIGH) | 4.0 µs | 4 µs = (160−128)×0.125 | ✓ |
| Phase (Q1↔Q2 centers) | 20.0 µs = 180° | 20 µs / 180° | ✓ |
| Both channels LOW together | never | none | ✓ |

**H-bridge (PA4=HA, PA5=LA, PA6=HB, PA7=LB):** ✅

| Quantity | Measured | Expected | ✓ |
|---|---|---|---|
| Period / frequency | 20.0 ms → 50.0 Hz | 20 ms / 50 Hz | ✓ |
| Active conduction / half-cycle | ~8.99 ms | 9000 µs | ✓ |
| Dead-time (all four HIGH) / swap | ~1.0 ms | 1000 µs | ✓ |
| Diagonal pairs | PA4+PA7 (HA+LB), then PA5+PA6 (HB+LA) | HA+LB / HB+LA | ✓ |
| Same-leg overlap (HA&LA or HB&LB) | never | none | ✓ |

> A probe-mapping note: an early H-bridge capture showed same-leg pairs — that was a **Ch5↔Ch7 logic-analyzer probe swap**, not a firmware fault. With the probes corrected, the capture shows the correct diagonals.

**Start-up sequence** (safe state → precharge → soft-start): ✅ confirmed **live on the analyzer** (the one-shot power-up events are impractical to export to CSV). From reset: all gates HIGH (off) and PA1 LOW immediately; then PA5+PA7 (LA+LB) LOW for ~5 ms with PA1 rising at its start; then 1 ms all-HIGH and the normal 9/1 ms unfold; and PB6/PB7 pulses widening 2 µs → 16 µs over ~0.5 s.

---

## Hardware Bring-up Trial Log (Stage 1 push-pull)

This log records the **power-stage** bench trials for Stage 1, as distinct from the firmware/logic-analyzer verification above. It is chronological; each trial notes goal, result, root cause and fix.

**Setup common to all trials:** EE20 transformer (Rev E: primary 22+22 turns bifilar SWG32 with centre tap, secondary 374 turns SWG38, n ≈ 17, Lm ≈ 110 µH per half), 2 × IRF3205, IR2110 used as a dual **low-side** driver (VS tied to GNDPWR), 6N137 optos, Blue Pill firmware at 25 kHz / 16 µs ON / 4 µs dead time. Bench: HTC DC3002 supplies (separate supplies for the gate drive and the transformer centre tap), Tektronix TBS1072B scope, logic analyzer, DMM.

### A. Gate-drive bring-up (2026-07-28)

**Goal:** a clean, fast 12 V gate drive to the two IRF3205s.

- **A1 — K1010 optocoupler rejected.** First wired with the output from the emitter (pin 3, follower mode) instead of the collector (pin 4) with a pull-up. More fundamentally, at 12 V the K1010 needs a load above ~4 kΩ to saturate but below ~320 Ω to reach 25 kHz bandwidth — both cannot be met at once. **Fix:** replaced by the 6N137 (~60 ns propagation delay).
- **A2 — Discrete BC547/BC557 totem-pole driver.** Three failure modes: (1) NPN/PNP emitters tied so both conducted → gate stuck at ~4 V; (2) NPN/PNP positions swapped → PNP permanently on and overheating; (3) 5 V logic driving a 12 V totem-pole → PNP never fully off, gate capped at ~5.4 V. **Fix:** replaced by the IR2110.
- **A3 — Software-toggled PB7.** PB7 was toggled in software from an ISR, racing PB6's hardware edge → shoot-through. **Fix:** center-aligned dual hardware channels (see [Push-pull PWM & dead-time](#push-pull-pwm--dead-time)).
- **A4 — Result:** gate drive verified at 25 kHz, 16 µs ON, 4 µs dead time, gate amplitude **11.4 V / 12.2 V**, no overlap.

_Scope capture: docs/trials/a4-gate-drive.jpg (to be added)_

### B. First power-stage attempts (2026-09-05)

- **B1 — First surge.** Centre tap connected straight to +12 V with no protection → core-saturation surge → **IR2110 damaged** (VCC–COM internal short; drew excess current with no input signal). **Fix:** IR2110 replaced.
- **B2 — Series DC-blocking caps (topology error).** A back-to-back 220 µF electrolytic pair placed **in series** with the centre-tap feed (intended as flux balancing) → only **2.5 V output**. Node voltages: 12 V before the caps, 3.1 V at the junction, 0 V at the centre tap; swapping the caps changed nothing. **Root cause:** the centre-tap feed carries one-way DC, so any series capacitor charges up and blocks it (series DC-blocking caps belong in half-bridges, not centre-tapped push-pulls). **Fix:** caps removed.
- **B3 — Series-choke trials in the centre-tap feed** (dual supplies):

| Trial | Vin    | Series element          | Ideal peak | Measured | Note |
|-------|--------|-------------------------|-----------|----------|------|
| 1     | 5 V    | resistor only           | 83 V      | 75 V     | 90 % — best result |
| 2     | ~4.7 V | 10 µH / 1 A choke       | 78 V      | 47 V     | choke over its rating (1.2 A) |
| 3     | ~12 V  | 300 µH / 1 A choke      | 202 V     | 28 V     | |
| 4     | ~12 V  | 2 × 300 µH in parallel  | 202 V     | 30 V     | halving L barely helped |
| 5     | 12.2 V | 100 µH / 1 A choke      | 202 V     | 28 V     | clean switching |

**Conclusion:** a series choke in a voltage-fed push-pull starves the transformer during the dead time (the choke current has nowhere to go and the centre tap flies). Chokes abandoned.

### C. Breadboard trials (2026-10-28)

Centre-tap feed through a 0.5 Ω / 5 W resistor; bridge output on the secondary:

- **0.5 Ω resistor, no bulk cap:** 5 V in, 1.1 A input, 50–55 V at the bridge. The supply would not go above ~7–8 V (current limit).
- **220 µF / 50 V added** (centre tap to GNDPWR): 5 V in, 1.5–1.6 A, centre tap 3.3 V, 0.6 V across the resistor, 63 V at the bridge.
- **1000 µF / 25 V instead:** still 63 V, same current → the capacitor was not the limit. Suspected breadboard contact resistance → power path moved to a soldered board. _(Later found: the excessive current was really fault D1 below.)_

### D. Soldered general-purpose board (2026-11-12 / 13)

- **D1 — Shorted-turn fault (bare diode clamps).** 3 V at the centre tap via 0.5 Ω (0.69 V drop), **1.54 A input**, 55 V at the bridge — current **5–7× too high** for no load (expected ≈ 0.2 A). Gates clean, no overlap; but the OFF drain only reached ≈ the tap voltage instead of 2 × tap voltage, with tall turn-off spikes. **Root cause:** plain UF4007 diodes from each drain straight to the centre tap (an RCD clamp without the R and C). Each diode conducted for the whole on-time of the *other* switch, shorting that half-winding — a shorted turn. **Fix:** diodes removed → input current fell to **0.01 A at 3 V**, OFF-drain plateau rose to 2 × Vtap. _Also found:_ scope probe on 1X while the scope was set to 10X → all readings 10× high.

  _Scope capture: docs/trials/d1-drain-shorted-turn.jpg (to be added)_

- **D2 — Centre-tap resistor removed.** The 0.5 Ω centre-tap resistor (R22) removed. No change in behaviour; the centre tap is now fed directly, with the bulk cap at the tap.

- **D3 — Proper RCD clamps fitted.** Each half: UF4007 (anode at drain) → 22 nF / 100 V film ‖ 1 kΩ 1 W → centre tap. At 12 V, no load: drain plateau ≈ 2 × Vin (~27 V), drain peaks **39.2 V / 45.6 V**; clamp-cap voltage 9.7 V (at its floor, so lowering R would not help); input **12 V / 0.05 A**; bridge output **267 V DC unloaded** (C7 = 1 µF / 450 V, bleeder 1.5 MΩ). The unloaded 267 V is C7 peak-charging on ringing, **not** the real output. The DMM's "232 V AC" on the secondary is not reliable at 25 kHz.

  _Scope capture: docs/trials/d3-drain-rcd-clamp.jpg (to be added)_

- **D4 — Load steps at ~12 V input** (5 W cement resistors in parallel across the bridge output; MDB10S bridge):

| Load (Ω)               | Vin / Iin       | Vout  | Pout   | Pin    | η     | Drain peaks Q1 / Q2 |
|------------------------|-----------------|-------|--------|--------|-------|---------------------|
| 33 k                   | 12.2 V / 0.15 A | 212 V | 1.36 W | 1.83 W | ~75 % | 42.4 / 46.4 V |
| 21 k                   | 12.0 V / 0.20 A | 204 V | 2.0 W  | 2.4 W  | ~83 % | 41.6 / 45.6 V |
| 16.25 k (33k ‖ 32k)    | 12.1 V / 0.25 A | 203 V | 2.5 W  | 3.0 W  | ~84 % | 36.0 / 39.2 V |
| 10.1 k (+27k)          | 12.1 V / 0.41 A | 203 V | 4.1 W  | 5.0 W  | ~82 % | 39.2 / 43.2 V |
| 7.2 k (+25k)           | 12.2 V / 0.57 A | 205 V | 5.8 W  | 7.0 W  | ~83 % | 40.8 / 44.8 V, rising to 45.6 / 48.8 V after ~3 min |
| 4.35 k (all six)       | 12.0 V / 1.03 A | 205 V | 9.7 W  | 12.4 W | ~78 % | 41.6 / 42.4 V |

  After the full-load run: MOSFETs and transformer cool; load resistors hot. Output regulation is flat (**203–205 V**) from 1.4 W to 9.7 W. From ~4 W up, each drain drops back low after its turn-off spike and only swaps at the other switch's turn-on → suspected slow reverse recovery of the MDB10S bridge (swap to 4 × UF4007 pending).

  _Scope capture: docs/trials/d4-load-steps.jpg (to be added)_

- **D5 — Transformer secondary waveform** (single probe across the secondary, 10X). At 2.0 V input / 0.02 A: **±37.6 V, 25.00 kHz, RMS 35.4 V**, symmetric (positive = negative, so flux balance is OK), effective ratio ≈ 18.3–18.8. At 12 V the swing (≈ ±225 V, scaling from the 2 V figures) exceeds the scope's 50 V/div range, so it was checked by offsetting the trace — symmetric and consistent with the ~205 V DC bus.

  _Scope capture: docs/trials/d5-secondary.jpg (to be added)_

### E. Result

**Stage 1 hardware validated** — 12 V → ~205 V DC at ~9.7 W, ~78–84 % efficient, switches and transformer cool.

### F. Open items for Stage 1

- Replace the MDB10S with 4 × UF4007 and repeat the 7.2 k and 4.35 k steps.
- Add P6KE39A TVS drain–source on each MOSFET (drain peaks up to 48.8 V vs the IRF3205's 55 V rating).
- Overcurrent protection: shunt + LM393 → IR2110 `SD` trip-and-latch (planned).
- 205 V bus is enough for the square-wave unfolder (~195 V RMS) but not for a 230 V sine (needs ~325 V): voltage doubler or more secondary turns, later.
- Opto-output capacitors C1/C6 (Stage 1) and C43–C46 (Stage 2) must be ≤100 pF or removed (0.1 µF there slows the gate signals); the opto bypass belongs on pin 8–pin 5.

### G. Lessons learned

- Never put a capacitor in series with a push-pull centre-tap feed (it carries DC).
- A clamp referenced to the centre tap needs its capacitor in series (RCD). A bare diode clamps the normal 2 × Vin and shorts the winding.
- Series chokes don't work in a voltage-fed push-pull's centre-tap feed.
- Breadboards can't carry the pulsed primary currents; solder the power path.
- Check the probe attenuation switch matches the scope setting.
- Measure the HV output under load; unloaded, C7 peak-charges to the ringing.
- DMM AC readings at 25 kHz are not trustworthy; use the scope.
- A supply current that doesn't scale with input voltage means a short, not magnetizing current.

---

## Before Applying High Voltage

The logic-analyzer verification covers the MCU gate signals only. Before pushing a stage to high voltage:

1. **Gate polarity through the chain — resolved for 6N137 + IR2110.** One net inversion, cancelled by the firmware's active-low drive (see [Gate-drive polarity](#gate-drive-polarity-active-low-for-6n137--ir2110)). Worth a one-time low-voltage bench check that all gates read OFF during the dead phases.
2. **Power-up / reset window.** Firmware forces all gates OFF and PA1 LOW before the clock switch, and the H-bridge stays disabled until after its precharge. The only residual exposure is the sub-millisecond window from reset release until `main()` runs (pins high-Z): add a **pull-up on each gate line** and a **pull-down on PA1**. Do not apply HV until the firmware is running.
3. **The IR2110 adds no dead-time of its own** — it passes HIN+LIN both-high straight to HO+LO both-on. Cross-conduction protection is entirely the firmware's; keep the dead-time invariants intact.
4. **Bootstrap supply.** Verify each IR2110's VS/VB on the scope; the high-side gate can't stay on indefinitely (bootstrap droop), fine at 25 kHz / 50 Hz.
5. **Differential output.** Scope node A vs node B across the bridge load for a clean ±HV 50 Hz square wave (not both nodes swinging in common mode) before connecting the transformer/load.

For the Stage 1 power-stage results already obtained, see the [Trial Log](#hardware-bring-up-trial-log-stage-1-push-pull).

---

## Hardware Architecture

### Solar panel array
- 5 cells in parallel → 6 V, 500 mA; a second identical set → 6 V, 500 mA; both in series → **12 V, 500 mA, ~6 W**.

### Charge controller — CN3722 (MPPT)
- Boost topology (solar Voc ~12 V → charge 14.4 V); charge current via R_prog; charge voltage via FB divider.
- Recommended inductor 22 µH shielded, DCR < 0.5 Ω. Input cap 10 µF + 100 nF; output cap 22 µF + 100 nF (25 V). Switching ~300 kHz.

| Component | Value | Formula |
|---|---|---|
| R_prog | ~2 kΩ | I_charge = 1000 / R_prog |
| R1 (FB divider) | 110 kΩ | V_charge = 1.2 × (1 + R1/R2) |
| R2 (FB divider) | 10 kΩ | Target V_charge = 14.4 V |
| Inductor | 22 µH | L = (Vin−Vout) / (I × fsw × ΔIL) |

### Battery protection — 4S LiFePO4 BMS
- Overcharge cutoff **14.6 V** (3.65 V/cell); over-discharge cutoff **10 V** (2.5 V/cell); short-circuit + overcurrent (10 A).
- ICs: DW01A (protection logic) + FS8205A (dual N-channel MOSFET switch). Wiring: Charger → BMS P+/P− | Battery → BMS B+/B−.

### Inverter — push-pull stage
- Center-tapped push-pull, 12.8 V primary side; two N-channel MOSFETs 180° out of phase with dead-time (see [Push-pull PWM & dead-time](#push-pull-pwm--dead-time)).
- Driven by STM32F103 TIM4 — PB6 (CH1) / PB7 (CH2), both hardware PWM, center-aligned; fixed 16 µs ON-time.
- Power stage schematic: `Pushpull_ckt.asc` (LTspice). Transformer: `EE20_Transformer_Spec_RevE_FINAL.docx`, `Transformer_Design_Revision_History.docx`.

### STM32 pin map

| Pin | Function | Direction |
|---|---|---|
| PB6 | TIM4_CH1 — Q1 push-pull gate (HW PWM, mode 1, trough) | AF push-pull |
| PB7 | TIM4_CH2 — Q2 push-pull gate (HW PWM, mode 2, peak) | AF push-pull |
| PA0 | ADC1_IN0 — former duty-control pot (unused; duty fixed) | Analog in (spare) |
| PA1 | H-bridge ENABLE → both IR2110 `SD` (active-high: HIGH = enabled) | GP push-pull |
| PA4 | H-bridge Leg A high-side gate (HA) | GP push-pull |
| PA5 | H-bridge Leg A low-side gate (LA) | GP push-pull |
| PA6 | H-bridge Leg B high-side gate (HB) | GP push-pull |
| PA7 | H-bridge Leg B low-side gate (LB) | GP push-pull |
| PC13 | Onboard LED (defined, not currently driven) | GP push-pull |
| OSC_IN / OSC_OUT | 8 MHz HSE crystal | — |

### Gate-drive isolation

Every gate signal is optically isolated, then buffered by **IR2110** drivers:

```
STM32 3.3V logic ──▶ 6N137 (isolation, inverts) ──▶ IR2110 (non-inverting) ──▶ MOSFET gate
```

- **6N137 optos — seven total:** two push-pull gates (PB6/PB7), four H-bridge gates (PA4–PA7), and one **H-bridge enable** (PA1 → the `SD` pins of both H-bridge IR2110s, a hardware enable independent of the gate signals). The polarity compensation is handled in firmware — see [Gate-drive polarity](#gate-drive-polarity-active-low-for-6n137--ir2110).
- **Why 6N137 (replacing the K1010):** the K1010 is a slow phototransistor opto; its µs-scale response smeared the fast 25 kHz edges and couldn't hold the 4 µs dead-time. The 6N137 is a logic-output opto (~10 Mbit/s, ~75 ns) that passes both stages cleanly.
- **6N137 per-device requirements:** Vcc 4.5–5.5 V with a 0.1 µF bypass right at pin 8 (mandatory); output pull-up ~1 kΩ to Vcc (open-collector output); input LED series resistor for ~5–15 mA; enable (pin 7) tied to Vcc if unused.
- **IR2110:** non-inverting (HIN→HO, LIN→LO), active-high `SD`. Each drives one half-bridge; the H-bridge uses two. It provides **no dead-time of its own** — that is the firmware's job.

### Optional monitoring — STM32 BluePill + INA219
- INA219 (I2C) in series on the charge line; shunt 0.1 Ω, 1 %, 1 W.
- Battery voltage divider for ADC: R1 = 100 kΩ, R2 = 27 kΩ (maps 0–14.6 V to 0–3.1 V), 100 nF across R2.

> No firmware yet. PA0 / ADC1 channel 0 is free again (pot removed), and `adc_driver.c` is already written for a single conversion on that channel — a battery-voltage read could reuse it directly. For *two* inputs it would need converting from its single-conversion sequence (`SQR1 L=0`) to a scan sequence.

### Output / load path
- 2 A blade fuse on the battery positive line.
- DC input → push-pull stage → step-up transformer → rectifier → HV DC bus → H-bridge unfolder (50 Hz) → 230 V AC load.

---

## Target Specifications

| Parameter | Value |
|---|---|
| Solar input | 12 V, ~500 mA, ~6 W |
| Solar cell configuration | 2 × (5 cells in parallel) in series |
| Each cell | 6 V, 100 mA, 0.5 W (70×70 mm polycrystalline) |
| Battery chemistry | LiFePO4 |
| Battery voltage (nominal) | 12.8 V |
| Battery capacity | 7 Ah (89.6 Wh) |
| Charge voltage | 14.4 V (CC/CV) |
| Max charge current | 500 mA |
| Load | 230 V AC, 10 W bulb |
| Target runtime | 3 hours |
| Push-pull switching frequency | 25 kHz (TIM4, center-aligned) |
| Push-pull ON-time (fixed) | 16 µs per switch (128 ticks, Rev E max) |
| Push-pull dead-time | 4 µs per gap (at the fixed 16 µs duty) |
| Push-pull soft-start | ~0.5 s ramp (2 µs → 16 µs ON) |
| H-bridge output frequency | 50 Hz (TIM3 unfolder) |
| H-bridge dead-time | 1 ms per polarity swap |
| H-bridge bootstrap precharge | 5 ms (LA+LB on before first swap) |
| H-bridge enable | PA1 active-high → both IR2110 `SD` |
| Stage 1 validated output | 12 V → ~205 V DC, ~9.7 W, ~78 % at full load |
| Inverter efficiency (assumed) | 80 % |
| Required battery energy | ~37.5 Wh |

---

## Build, Flash & Toolchain

Development host is **Windows 11**. Only `arm-none-eabi-gcc`, CMake and a generator (Make or Ninja) are needed to build; OpenOCD is needed only to flash/debug. Nothing depends on the STM32 HAL/LL or STM32CubeMX — the vendor headers under [include/CMSIS/](include/CMSIS/) and [include/Device/](include/Device/) are the only external code.

| Tool | Version in use | Location / source |
|---|---|---|
| Arm GNU Toolchain (`arm-none-eabi-gcc`) | 15.2.Rel1 — gcc 15.2.1 | `C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\` |
| `arm-none-eabi-gdb` | 16.3.90 | ships with the Arm toolchain |
| CMake | 4.3.3 | `C:\Program Files\CMake\` (`CMakeLists.txt` requires ≥ 3.20) |
| GNU Make (`mingw32-make`) | 4.2.1 | `C:\CTOOLS\mingw32\` — backs the "MinGW Makefiles" generator |
| Ninja | 1.13.2 | winget — optional alternative generator |
| OpenOCD | 0.12.0-7 (xPack) | winget (`xpack-dev-tools.openocd-xpack`) |
| Git | 2.54.0 | `C:\Program Files\Git\` |
| VS Code + `cortex-debug` | — | debug integration, see [.vscode/launch.json](.vscode/launch.json) |

### Build

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..     # or "Unix Makefiles" / "Ninja"
cmake --build .
```

Build config (all in [CMakeLists.txt](CMakeLists.txt)): cross-compile setup (`CMAKE_SYSTEM_NAME Generic`, `CMAKE_SYSTEM_PROCESSOR arm`, before `project()`); `CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY` to skip the hosted link test; flags `-mcpu=cortex-m3 -mthumb -Wall -ffunction-sections -fdata-sections -O0 -g`; link with the custom script, `-Wl,--gc-sections`, a `.map`, and `--specs=nosys.specs`; `STM32F103xB` defined project-wide. Produces `.elf` + `.bin` and prints an `arm-none-eabi-size` summary:

```
   text    data     bss     dec     hex  filename
   2508      20    1976    4504    1198  inverter_firmware.elf
```

### Flash / Debug

```powershell
cmake --build build; if ($?) { openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/inverter_firmware.elf verify reset exit" }
```

Debugging is configured in [.vscode/launch.json](.vscode/launch.json) via `cortex-debug` + OpenOCD, ST-Link probe, `runToEntryPoint: main`. [.vscode/c_cpp_properties.json](.vscode/c_cpp_properties.json) carries two IntelliSense configs — **STM32** (the real one: `arm-none-eabi-gcc`, C11, `STM32F103xB`) and a **Win32** fallback. Select "STM32" for correct register IntelliSense.

---

## PCB Design Plan

- **Tool:** KiCad. **Layers:** 2 (top: components + routing, bottom: solid ground plane). **Fab:** JLCPCB (Gerber export).

**Layout priorities (in order):**
1. Switching loop (CN3722 SW ↔ inductor ↔ output cap) — keep tight, minimize enclosed area.
2. Ground plane on the bottom layer, stitched with vias around the switching section.
3. Analog ground (FB divider, INA219 sense) star-connected to power ground at the CN3722 GND pin.
4. High-current traces (battery, fuse, charge output): ≥ 1.5 mm at 1 oz copper.
5. STM32 and I2C traces routed away from the switching section.

**Package choices (for hand soldering):** CN3722 SOP-8; INA219 SOT-23-8; load-switch MOSFETs SOT-23 or D-PAK (TO-252); passives 0805 minimum.

**Datasheets to study (Phase 1):** CN3722 (MPPT solar charger, Consonance); DW01A (Li protection); FS8205A (dual N-channel MOSFET for BMS).

---

## Bill of Materials

| Component | Value / Part | Source |
|---|---|---|
| Solar cells | 70×70 mm, 6 V 100 mA polycrystalline (×10) | Amazon India |
| Battery | LiFePO4 12.8 V 7 Ah | Amazon India / Keltron |
| Charge controller IC | CN3722 (SOP-8) | Robu.in / Lamington Road |
| BMS module or ICs | 4S LiFePO4 BMS (DW01A + FS8205A) | Lamington Road |
| Blocking diode | 1N5822 Schottky | Lamington Road |
| TVS diode (charger) | P6KE15A | Lamington Road |
| Inductor | 22 µH shielded, DCR < 0.5 Ω (CDRH6D28 or similar) | Lamington Road |
| Input capacitor | 10 µF ceramic, 25 V, X5R/X7R | Lamington Road |
| Output capacitor | 22 µF ceramic, 25 V, X5R/X7R | Lamington Road |
| R_prog | 2 kΩ, 1 %, 0805 | Lamington Road |
| FB divider R1 / R2 | 110 kΩ / 10 kΩ, 1 %, 0805 | Lamington Road |
| Voltage divider R1 / R2 | 100 kΩ / 27 kΩ, 1 %, 0805 | Lamington Road |
| INA219 | SOT-23-8 or module | Amazon India |
| Shunt resistor | 0.1 Ω, 1 %, 1 W | Lamington Road |
| Fuse + holder | 2 A blade automotive | Local / Lamington Road |
| Inverter MOSFETs | 2 × IRF3205 (push-pull) | Lamington Road |
| Transformer | EE20, Rev E (22+22 : 374, bifilar SWG32 / SWG38) | custom-wound |
| Gate driver | **IR2110** — ×2 for the H-bridge (one per leg) + push-pull driver | Lamington Road |
| Gate-drive optocoupler | **6N137** (~10 Mbit/s), ×7 — 6 gate lines + 1 H-bridge enable (PA1 → IR2110 `SD`); **replaces the slow K1010** | Lamington Road |
| Opto output pull-up | ~1 kΩ, ×7 | Lamington Road |
| Opto Vcc bypass cap | 0.1 µF ceramic, ×7 (at each 6N137 pin 8) | Lamington Road |
| Bootstrap diode + cap | fast diode (e.g. UF4007) + ~1 µF, per IR2110 high-side | Lamington Road |
| Gate resistors | ~10–22 Ω per MOSFET gate | Lamington Road |
| Drain RCD clamp | UF4007 + 22 nF / 100 V film ‖ 1 kΩ 1 W, per half-primary | Lamington Road |
| Secondary rectifier | MDB10S (→ 4 × UF4007 planned) | Lamington Road |
| HV bus cap / bleeder | 1 µF / 450 V (C7) + 1.5 MΩ bleeder | Lamington Road |
| Drain TVS (planned) | P6KE39A, per MOSFET | Lamington Road |
| Duty-control pot | 200 Ω trimmer/rotary — *bench-tuning only; not needed now duty is fixed* | Lamington Road |
| STM32 BluePill | STM32F103C8T6 | Amazon India / Robu.in |
| ST-Link V2 programmer | ST-Link V2 clone or genuine | Amazon India / Robu.in |

---

## Phases

- [x] **Phase 0** — Concept and load/battery sizing
- [ ] **Phase 1** — Datasheet study, component value calculations, schematic (KiCad)
- [ ] **Phase 2** — PCB layout (KiCad), Gerber generation, send to JLCPCB
- [ ] **Phase 3** — Assembly and bring-up
  - [x] Stage 1 push-pull hardware bring-up on general-purpose board — validated to ~10 W (see [Trial Log](#hardware-bring-up-trial-log-stage-1-push-pull))
  - [ ] Stage 2 H-bridge hardware bring-up
- [x] **Phase 4 (started early)** — Two-stage inverter firmware working and logic-analyzer verified: center-aligned push-pull (TIM4, 25 kHz, fixed 16 µs duty, 4 µs dead-time, ~0.5 s soft-start) + 50 Hz H-bridge unfolder (TIM3, PA4–PA7, 1 ms dead-time) with PA1 enable and 5 ms precharge, glitch-free safe-state power-up, active-low gate drive for 6N137 → IR2110; no shoot-through on either stage
- [ ] **Phase 5** — WiFi control + LDR automation (STM32 + ESP8266/ESP32)

---

## Tools Used

**Firmware:** Arm GNU Toolchain (`arm-none-eabi-gcc` 15.2.Rel1); CMake 4.3.3 + MinGW Make 4.2.1 (or Ninja 1.13.2); OpenOCD 0.12.0 (xPack) + ST-Link; `arm-none-eabi-gdb` 16.3.90 (via cortex-debug); VS Code + cortex-debug; Git 2.54.0.

**Hardware / design:**
- KiCad — schematic and PCB layout (Phases 1–2, not yet started).
- LTspice — push-pull power stage simulation (`Pushpull_ckt.asc`).
- Logic analyzer — verified both stages' timing, dead-time and active-low polarity, the PA1 enable, and the start-up sequence; no shoot-through (2026-11-16).
- Oscilloscope (Tektronix TBS1072B) + DMM — bench bring-up and the Stage 1 power trials (drain, secondary, load-step measurements — see [Trial Log](#hardware-bring-up-trial-log-stage-1-push-pull)).

**Not used, deliberately:** STM32 HAL/LL, STM32CubeMX/CubeIDE, any RTOS. Everything is register-level C against the CMSIS + ST device headers in [include/](include/).

---

## Notes

- **Component sourcing:** Lamington Road, Mumbai for most passives and ICs; Amazon India for modules and the battery.
- The firmware is being prototyped ahead of the charger/BMS PCB (Phases 1–3) so the transformer and switching scheme can be validated on the bench first.
- **TODO (code tidy):** add a typedef header so all `uint16_t` names are defined properly — the register code reads messy without it.
