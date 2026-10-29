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
- No `EEPROM_SETTINGS` — see §8.

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
   prints as `Kp`, `Ki`, `Kd` — but note the config macros are `DEFAULT_CHAMBER_KP`
   and so on. With no EEPROM the result must be pasted into `Configuration.h` and
   reflashed; `M500` does not exist here.

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

- **No EEPROM.** The GD32_MFL HAL has no internal-flash emulation; its only
  fallbacks are an SD card or an external I2C/SPI chip, and neither is fitted.
  Enabling `EEPROM_SETTINGS` is a hard build error, so `M500`/`M501`/`M503` do
  not exist. PID gains are compile-time: tune, paste, reflash.
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
  `HAS_HOTEND`-class guard, so no zero-length arrays are emitted;
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
- `EEPROM_SETTINGS` really is a build error for this HAL, which is why §8 exists.

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
