# Chamber controller — Creality V4.2.2 (GD32F303RET6)

Marlin `bugfix-2.1.x` used as a standalone chamber controller. The board drives a
TEC, a stir fan, two thermistors and a serial link to an ESP32 host. There is no
printer attached: no hotend, bed, extruder, steppers, endstops, display or SD
card.

This uses Marlin's native Chamber feature (`M141` / `M191`), not a repurposed
hotend or bed.

**The TEC is used as a heater.** Its hot face is inside the chamber, so more
drive means a hotter chamber and Marlin's chamber PID has the correct sign. The
cold face is outside the box and its exterior fan runs continuously, off-board.

---

## 1. Wiring

| Board connector | MCU pin | Marlin macro | Function |
|---|---|---|---|
| `HEATER1` | PA1 | `HEATER_CHAMBER_PIN` | TEC drive (hot face into the chamber) |
| `FAN` | PA0 | `FAN0_PIN` | Stir fan, driven by Marlin's `CHAMBER_FAN` |
| `TB1` | PC4 | `TEMP_CHAMBER_PIN` | Chamber air NTC — controls the heater and carries the trip |
| `TH1` | PC5 | `TEMP_1_PIN` | TEC hot-plate NTC — redundant monitor |
| `EXP3-03/04` | PB10/PB11 | `SERIAL_PORT 2` (USART2) | Host UART, 250000 baud |

`TB1` and `TH1` are just the two ADC inputs; the silkscreen names them bed and
hotend. Both are 100K NTCs — only the macro decides which is which.

The stir fan is not optional. Without air movement the hot plate forms a plume,
PC4 sits in cooler air, the PID keeps driving, and the sample cooks while the
sensor reads 76 °F. `CHAMBER_FAN` runs PA0 whenever a chamber target is set and
stops it when `M141 S0` clears the target.

**Unplug the stock 12864 display.** Its EXP3 header carries PB10/PB11, which are
now the host UART.

**Baud rate is 250000** (`Configuration.h`). Match it on the ESP32.

---

## 2. Files

| File here | Goes to |
|---|---|
| `Configuration.h` | `Marlin/Configuration.h` |
| `Configuration_adv.h` | `Marlin/Configuration_adv.h` |
| `pins/pins_CHAMBER_V422_GD32.h` | `Marlin/src/pins/gd32f3/pins_CHAMBER_V422_GD32.h` |

Two source edits register the new board. Without them the pins file is never
read.

`Marlin/src/core/boards.h` — under *GD32 ARM Cortex-M4*:

```c
#define BOARD_CHAMBER_V422_GD32       7402  // Creality V4.2.2 as a chamber controller (GD32F303RE)
```

`Marlin/src/pins/pins.h` — in the same GD32F303RE block:

```c
#elif MB(CHAMBER_V422_GD32)
  #include "gd32f3/pins_CHAMBER_V422_GD32.h"   // GD32F303RE                           env:GD32F303RE_creality_mfl
```

Nothing else in the tree changes. The board reuses the existing
`env:GD32F303RE_creality_mfl` build environment unchanged.

---

## 3. Build and flash

```bash
pio run -e GD32F303RE_creality_mfl
```

Do **not** build with `mftest`: it regenerates `Configuration.h` and would
replace the one in this folder.

The environment links at `0x08007000` and emits `firmware-<timestamp>.bin`, so
write the image at **0x08007000** (ST-Link / STM32CubeProgrammer), or let the
PlatformIO upload target do it. Writing at 0x08000000 overwrites the Creality
bootloader and the board will not start.

---

## 4. Temperature targets

Marlin has no Fahrenheit support anywhere, every limit is Celsius, and the
setpoint quantises to whole degrees — 1 °C is 1.8 °F, so you cannot ask for
exactly 75 °F.

| °C | °F | Meaning |
|---|---|---|
| 24 | 75.2 | bottom of the sample's ideal band |
| 27 | 80.6 | top of the ideal band, and the highest target settable (32 − 5 overshoot) |
| 32 | 89.6 | hard trip: heater off, needs `M999` |
| 38 | 100.4 | sample dies above here — the 6 °C of trip margin is deliberate |

`CHAMBER_MAXTEMP 32` is three things at once: the `MAXTEMP` trip, the upper gate
on heater output, and the top of the chamber thermistor's ADC window. Lowering
it tightens all three together. A trip is not damage — it fails cold, which is
the safe direction here — but it latches until `M999` and the chamber then
drifts to ambient.

Run inside the band: `M141 S26` is 78.8 °F and sits mid-band. `M141 S27` is
allowed and is the ceiling; `S28` is silently clamped.

---

## 5. What is enabled

`Configuration.h`

- `EXTRUDERS 0` → `HOTENDS 0`, which compiles out `PIDTEMP`,
  `PREVENT_COLD_EXTRUSION` and `THERMAL_PROTECTION_HOTENDS` automatically.
- `TEMP_SENSOR_CHAMBER 1` on PC4, `PIDTEMPCHAMBER`, `THERMAL_PROTECTION_CHAMBER`.
- `TEMP_SENSOR_REDUNDANT 1` with `SOURCE E1`, `TARGET CHAMBER`. PC5 is read
  straight from `TEMP_1_PIN`; `TEMP_SENSOR_1` stays `0`.
- One dummy X axis. Marlin cannot build with `NUM_AXES 0` (`SanityCheck.h`
  indexes the step-rate arrays unconditionally), so X is declared and never used.
- `EEPROM_SETTINGS` on the board's 2K 24C16 (`PA11`/`PA12`) — see §8.

`Configuration_adv.h` differs from the stock Creality V4.2.2 example in the places
below. Only the first three are electrical; the rest are build requirements,
safety timing, or the stir fan.

| Setting | Value | Why |
|---|---|---|
| `HEATER_CHAMBER_PIN` | `PA1` | the TEC drive MOSFET |
| `CHAMBER_FAN` | on, index `0`, mode `0`, base `200` | PA0 is a stir fan and must run whenever heating |
| `CHAMBER_FAN_MODE` | `0` | fixed duty > a temperature curve; stirring is not extraction |
| `THERMAL_PROTECTION_CHAMBER_PERIOD` | `120` | chamber air heats far too slowly for the stock window |
| `THERMAL_PROTECTION_CHAMBER_HYSTERESIS` | `3` | same |
| `WATCH_CHAMBER_TEMP_PERIOD` | `300` | `2 °C / 120 s` is unreachable at 25 % duty from 65 °F |
| `WATCH_CHAMBER_TEMP_INCREASE` | `1` | same; its failure mode is a cold chamber |
| `HOMING_BUMP_MM` / `HOMING_BUMP_DIVISOR` | one element | arrays must match `NUM_AXES` (1) |
| `AXIS_RELATIVE_MODES` | one element | must match `LOGICAL_AXES` (1) |
| `BABYSTEPPING` | off | there is no Z axis to babystep |
| `EMERGENCY_PARSER` | on | `M112` works while a move is queued |
| `AUTO_REPORT_REDUNDANT` | on | `M155` auto-reports include the hot plate |
| `FAN_KICKSTART_TIME` | `300` | starts a stalled 24 V fan |

`MAX_CHAMBER_POWER` is `64` of 255 — 25 % duty. That is the real control on this
machine, not the PID gains. It applies even in the loop's bypass branch
(`output_pow = MAX_POW`, `temperature.h:219`), so it is a true ceiling.

The PID gains are Marlin's published example for a 200 W resistive element
through an SSR. They are **not** a tune for a Peltier. A TEC is worse behaved
than a resistive heater: its plate stores energy, so cutting drive to zero does
not stop heat entering the chamber, it only stops adding to it. Expect overshoot
of a degree or two past target and do not read that as a fault.

---

## 6. Commissioning

First power-up? Work through §10 in order — it is the safe sequence. This
section is the G-code reference behind it.

Send these over the host UART. Every reply ends in `ok`.

1. **Both sensors alive.** `M105` returns the chamber air as `C:`; `M105 R` adds
   the hot plate as `R:`. Cold, with the TEC never powered, the two should agree
   within a few °C. If either reads `0` or wildly wrong, stop — the matching
   thermistor table in `Configuration.h` is wrong for that probe.

2. **Stir fan.** It is automatic now:

   ```
   M141 S26      fan starts at PWM 200
   M141 S0       fan stops
   ```

   `M106` / `M107` do **not** control PA0 any more — Marlin owns that fan.

3. **Heater, supervised.** Put a thermometer through the door, not just the
   probe, then:

   ```
   M141 S26
   M105          (repeat until it settles, then:)
   M141 S0
   ```

   `M141 S0` turns the heater off. `M191 S26` does the same but blocks until the
   chamber is within `TEMP_CHAMBER_WINDOW` for `TEMP_CHAMBER_RESIDENCY_TIME`.
   `M141 S40` is accepted but silently clamped to 27 °C, the ceiling.

4. **Autotune, if you want it.** There is 6 °C of headroom between a 26 °C target
   and the 32 °C trip, so autotune can overshoot without tripping — but at a
   ~8 °C delta above ambient it may still refuse to converge, and a Peltier is
   not the load the shipped gains were written for.

   ```
   M303 E-2 S26 C8
   ```

   `E-2` is Marlin's internal id for the chamber heater (`H_CHAMBER`). The result
   prints as `Kp`, `Ki`, `Kd` — note the config macros are `DEFAULT_CHAMBER_KP`
   and so on. Feed the numbers back with `M309` and keep them with `M500`:

   ```
   M309 P<kp> I<ki> D<kd>
   M500
   M503          (read them back; they survive power-off)
   ```

5. **Set `TEMP_SENSOR_REDUNDANT_MAX_DIFF`.** This is the one value that must be
   measured, not guessed:

   - Run the chamber at 26 °C and let it stabilise.
   - `M105 R` — note `C:` (chamber air) and `R:` (TEC hot plate).
   - The plate always sits higher. `gap = R − C`.
   - Set `TEMP_SENSOR_REDUNDANT_MAX_DIFF` to `gap + 25`, reflash.

   `80` ships as a placeholder. It will not fire until the plate is roughly 80 °C
   above the air — about 106 °C at a 26 °C chamber. That is a plausibility check,
   **not** TEC protection: it is a differential, so it can never enforce an
   absolute limit on the plate however you set it.

   When the two readings differ by more than that, Marlin stops the machine,
   disables the chamber heater and reports `Redundant temp sensor` on the host.

---

## 7. What to expect from the numbers

- **The ambient range now fits inside the band.** Ideal is 24–27 °C (75.2–80.6 °F)
  and your room is 65–75 °F, so at the top of the ambient range the box needs no
  heat at all and the heater idles. All the work is at the 65 °F end, where it
  adds roughly 13 °F.
- **It cannot cool.** With the cold face outside and the hot face inside, the TEC
  only ever heats the chamber. If the room goes above the band the chamber
  follows it up with the heater doing nothing. It would take a ~90 °F room to
  reach the 89.6 °F trip that way, so this is a hot-day caveat, not a routine
  one — but it is a property of the machine, not of the firmware.
- **The cold face will condense.** Below the local dew point it will sweat or
  frost on the exterior side. Plan for drainage and do not let it drip onto the
  electronics.

---

## 8. Limits worth knowing

- **EEPROM is external, and shared with the old firmware.** The GD32F303RET6 has
  no data-EEPROM and GD32_MFL has no flash emulation, so this build uses the 2K
  24C16 the board carries on I2C-0. `pins_CREALITY_V4.h` selects it by itself
  when `EEPROM_SETTINGS` is on. If the board last ran a printer, that chip still
  holds a printer's settings blob: run `M502` then `M500` once on first boot
  (§10). If `M500` errors while `M501` reads fine, the chip's WP pin is strapped.
- **Losing the blob is not a brick.** A blank or wiped chip, or a version/CRC
  mismatch, makes Marlin re-initialise the defaults from `Configuration.h`
  (`EEPROM_AUTO_INIT`). Tuned gains are the only thing you would lose.
- **One dummy X axis.** Unavoidable in current Marlin. It costs a few hundred
  bytes and some unused `G0`/`G28` surface. Do not send `G28` — nothing is wired
  to the X endstop.
- **The redundant sensor is monitoring, not a safety device.** Both probes are
  software, both share the MCU. A shorted thermistor, a stuck MOSFET or a hung
  firmware defeats it, and being a differential it is blind to a plate that is
  too hot while the air is proportionally hot too.
- **Fit a hardware thermal cutout** in series with the TEC supply. Off-the-shelf
  normally-closed bimetals (KSD9700 family) start around 40 °C, which is above
  your 38 °C (100.4 °F) damage point — so treat that as component and fire
  protection, not sample protection. There is no cheap mechanical thermostat
  between the 27 °C working ceiling and 38 °C.

---

## 9. Verification status

Checked by reading `bugfix-2.1.x` source, not by compiling:

- every macro these configurations reference exists, or is defaulted, in
  2.1.x — including the ones that have no default anywhere in the source and
  therefore had to be supplied here (`PID_FUNCTIONAL_RANGE`,
  `TEMP_CHAMBER_HYSTERESIS`, `TEMP_CHAMBER_WINDOW`,
  `TEMP_CHAMBER_RESIDENCY_TIME`, `X_MIN_ENDSTOP_HIT_STATE`);
- macros that *only* apply to a hotend (`TEMP_HYSTERESIS`, `TEMP_WINDOW`,
  `TEMP_RESIDENCY_TIME`, `BANG_MAX`, `PREHEAT_TIME_HOTEND_MS`) are only ever
  referenced inside `HAS_HOTEND`/`HAS_TEMP_HOTEND` blocks, so `EXTRUDERS 0`
  keeps every one of them out of the build;
- all three files have balanced `#if`/`#endif` nesting (checked mechanically);
- array settings are sized for `NUM_AXES == 1` and `LOGICAL_AXES == 1`, and
  every `[HOTENDS]`/`[EXTRUDERS]` array in `temperature.cpp` is inside a
  `HAS_HOTEND`-class guard. The `[HOTENDS]`/`[EXTRUDERS]` arrays in
  `settings.cpp` *do* collapse to zero length once `EEPROM_SETTINGS` is on; GCC
  accepts that as an extension, the mfl environment sets no `-Werror`, and every
  loop over them runs zero times;
- no macro is defined twice across `Configuration.h` and `Configuration_adv.h`
  (checked mechanically; the only repeats inside `Configuration_adv.h` are the
  stock `#if MODE ==` fan blocks, which are mutually exclusive);
- `CHAMBER_FAN_MODE 0` needs only `CHAMBER_FAN_BASE`, and `CHAMBER_FAN_INDEX 0` is
  a real fan because `FAN0_PIN` exists on this board (`FAN_COUNT == 1`);
- `flag_chamber_excess_heat` — the one static that enabling `CHAMBER_FAN` brings
  into scope — is definitely used by the `PIDTEMPCHAMBER` branch at
  `temperature.cpp:2195`, so it is not an unused-variable build risk;
- `CHAMBER_MAXTEMP 32` is not constrained by any sanity check, and the
  chamber ADC window is derived from it, so the trip and the sensor range stay
  consistent;
- `TEMP_SENSOR_REDUNDANT_SOURCE E1` resolves through `TEMP_1_PIN`, and the abort
  path cuts `TEMP_SENSOR_REDUNDANT_TARGET` (the chamber heater);
- `SERIAL_PORT 2` is USART2 = PB10/PB11 in the GD32_MFL HAL (0-based serials);
- `EEPROM_SETTINGS` resolves to the board's 24C16, not to a HAL fallback:
  `NO_EEPROM_SELECTED` in `pins_CREALITY_V4.h` defines `IIC_BL24CXX_EEPROM`
  (`MARLIN_EEPROM_SIZE 0x800U`, `PA11`/`PA12`), and GD32_MFL ships
  `eeprom_bl24cxx.cpp` for it. `FLASH_EEPROM_LEVELING` is what the HAL rejects.

**Not verified: it has never been compiled or flashed.** No ARM toolchain or
PlatformIO was available where this was written. Treat it as "should build" and
expect to fix the first compiler message, not as a tested image.

The shortest path from here to a known-good build is:

```bash
python3 -m ensurepip
python3 -m pip install platformio
pio run -e GD32F303RE_creality_mfl
```

Send back the first error and it gets fixed.

---

## 10. First run checklist

For the first binary that builds. Work down the list; stop at the first step that
fails. Nothing here needs the ESP32 firmware finished — a USB-serial adapter on
`EXP3-03/04` plus a terminal at 250000 does the job.

### A. Before power

- [ ] Unplug the stock 12864 display. Its `EXP3` carries PB10/PB11 (§1).
- [ ] Leave the TEC unplugged from `HEATER1` for now. The drive is checked first,
      the load later.
- [ ] Nothing in the X/Y/Z stepper sockets. X exists in firmware but the pins are
      not wired to anything.
- [ ] Exterior (cold-face) fan on its own supply, running. It is not on this board.
- [ ] Flash at **0x08007000**, not 0x08000000 (§3).

### B. First boot — prove comms and persistence

- [ ] `M115` answers, no reset loop. A first-boot `EEPROM version mismatch`
      followed by `EEPROM initialized` is expected: `EEPROM_AUTO_INIT` replaces
      whatever the chip held with the defaults from `Configuration.h`.
- [ ] **`M502` then `M500`, once.** Still do it. The automatic path only fires
      when the old blob fails validation; it will not clean up a valid blob left
      by an earlier build of this firmware. `M502` restores this file's defaults,
      `M500` writes them, and everything after this point starts from a known state.
- [ ] `M500` must answer `ok`. If it answers `EEPROM write error` while `M501` and
      `M503` read fine, the chip's write-protect pin is strapped on that board —
      tell me, and we fall back to compile-time gains.
- [ ] `M503` — note the `M309 P.. I.. D..` line. That is the baseline to compare
      against later.
- [ ] Power-cycle, `M503` again. Identical = persistence works.
- [ ] `M112`, then `M999`. Proves the emergency parser is in the build.

### C. Sensors, cold, heater off

- [ ] `M105` → `C:` within a couple of °C of the room.
- [ ] `M105 R` → `R:` present, and close to `C:` (TEC unpowered, both at ambient).
      `R:` missing or `0` means the PC5 probe or its table is wrong.
- [ ] Compare `C:` against a separate thermometer sitting next to the probe. The
      working band is only 3 °C wide (§4), so a table that reads 2 °C high is the
      difference between 26 °C and 28 °C in the box. If it is off by more than
      ~1 °C, stop here — we pick a different thermistor table.
- [ ] Warm the chamber probe past 32 °C to test the trip — a probe in a sealed bag
      of ~40 °C water is repeatable; a closed fist is marginal. Expect a MAXTEMP
      report and Marlin stopping the machine; `M999` clears it. Safe with the TEC
      unplugged.

### D. Stir fan

- [ ] `M141 S26` → PA0 fan spins up. `M141 S0` → it stops.
- [ ] `M106` / `M107` do nothing to it. That is correct, not a fault (§6.2).

### E. Heater drive, still no TEC

- [ ] `M141 S26`, meter across `HEATER1`. The output should switch, averaging
      towards ~25 % of the supply as the PID fights a cold probe.
- [ ] Leave it: nothing heats, so after `WATCH_CHAMBER_TEMP_PERIOD` (300 s) Marlin
      reports `Heating failed` and cuts the heater. **That is the desired result**
      — it proves the cold-failure path before the TEC is ever connected.
- [ ] `M999`, then `M141 S0`.

### F. First real heat

- [ ] TEC connected, hot face into the chamber, cold face outside with its fan.
- [ ] A thermometer inside the box, next to the probe. **No sample for this run.**
- [ ] Hand on the power switch. `M141 S24` — the lowest target, 75.2 °F.
- [ ] `M105` every 30 s for 15 minutes. `C:` climbs slowly and settles near 24;
      `R:` stays above `C:`. A °C or two of overshoot is normal (§5).
- [ ] If `C:` passes ~28 °C, cut it: `M141 S0`, or `M112`.
- [ ] `M141 S0` when done, confirm the fan stops and the output is off.

### G. Redundant sensor

- [ ] At a steady 26 °C, `M105 R`, record `gap = R − C`.
- [ ] Set `TEMP_SENSOR_REDUNDANT_MAX_DIFF` to `gap + 25` in `Configuration.h` and
      reflash. That is the one number in this build that has to be measured (§6.5).
- [ ] Optional: unplug the TH1 probe → `R:` reads open, the chamber heater is cut
      and the host gets `Redundant temp sensor`. Proves the abort path.

### H. Tune, then persist

- [ ] `M303 E-2 S26 C8`. It may well fail to converge at an 8 °C delta.
- [ ] If it converges: `M309 P.. I.. D..`, `M500`, power-cycle, `M503` to confirm
      the new gains came back.
- [ ] If it fails: leave the shipped gains, and lean on `MAX_CHAMBER_POWER`.

### Things that look like faults and are not

| Symptom | Reality |
|---|---|
| `M141 S28` is accepted but settles at 27 | clamped to `CHAMBER_MAXTEMP − CHAMBER_OVERSHOOT` |
| `R:` higher than `C:` | the plate is hotter than the air; that is the design |
| 1–2 °C overshoot | stored heat in the plate (§5) |
| `Heating failed` with no load attached | the 300 s watchdog doing its job (§E) |
| `M503` shows `M309` values that differ from `DEFAULT_CHAMBER_KP` | EEPROM wins over the file, by design |

### One thing that is not protected

If the ESP32 dies mid-run, the chamber keeps holding its last target. Marlin has
no host-timeout for `M141`. Whatever sends targets should also send `M141 S0` on
shutdown, and a hardware cutout is still the only defence against a stuck
MOSFET (§8).
