# Snapshot 2026-10-29

Frozen copy of the chamber-controller configuration as handed over for the first
build. Nothing here is read by the build — the working files are one level up.

This revision supersedes the same-day draft that used a 27 °C trip; that draft
was never built or flashed.

## Target

Sample band 24–27 °C (75.2–80.6 °F). Damage above 38 °C (100.4 °F). Room is
65–75 °F. Heater fails cold, which costs no sample, so the design leans hard on
over-temperature protection and a low power cap.

| Temperature | °C | °F | Role |
|---|---|---|---|
| bottom of band | 24 | 75.2 | — |
| top of band / highest settable target | 27 | 80.6 | `CHAMBER_MAXTEMP − CHAMBER_OVERSHOOT` |
| hard trip | 32 | 89.6 | heater off, latches until `M999` |
| damage | 38 | 100.4 | 6 °C of margin below the trip |

## Settings

| File | Macro | Value | Note |
|---|---|---|---|
| `Configuration.h` | `CHAMBER_MINTEMP` | 5 | heater never runs below this |
| `Configuration.h` | `CHAMBER_MAXTEMP` | 32 | trip, heater gate, top of ADC window |
| `Configuration.h` | `CHAMBER_OVERSHOOT` | 5 | sets ceiling to 27 °C |
| `Configuration.h` | `TEMP_CHAMBER_HYSTERESIS` | 1 | |
| `Configuration.h` | `TEMP_CHAMBER_WINDOW` | 1 | |
| `Configuration.h` | `TEMP_CHAMBER_RESIDENCY_TIME` | 10 | |
| `Configuration.h` | `MAX_CHAMBER_POWER` | 64 | 25 % duty, true ceiling |
| `Configuration.h` | `TEMP_SENSOR_REDUNDANT_MAX_DIFF` | 80 | placeholder, measurement needed |
| `Configuration.h` | `PID_FUNCTIONAL_RANGE` | 20 | no default in source |
| `Configuration_adv.h` | `HEATER_CHAMBER_PIN` | PA1 | TEC drive |
| `Configuration_adv.h` | `CHAMBER_FAN` | on, idx 0, mode 0, base 200 | PA0 stir fan |
| `Configuration_adv.h` | `THERMAL_PROTECTION_CHAMBER_PERIOD` | 120 | |
| `Configuration_adv.h` | `THERMAL_PROTECTION_CHAMBER_HYSTERESIS` | 3 | |
| `Configuration_adv.h` | `WATCH_CHAMBER_TEMP_PERIOD` | 300 | |
| `Configuration_adv.h` | `WATCH_CHAMBER_TEMP_INCREASE` | 1 | |

## Revision history

Changes in this revision (triggered by the corrected 24–27 °C sample band):

| Macro | Was | Now |
|---|---|---|
| `CHAMBER_MAXTEMP` | 27 | 32 |
| `CHAMBER_OVERSHOOT` | 1 | 5 |

Changes made earlier the same day, carried forward unchanged:

| Macro | Was | Now |
|---|---|---|
| `TEMP_CHAMBER_HYSTERESIS` | 3 | 1 |
| `MAX_CHAMBER_POWER` | 255 | 64 |
| `WATCH_CHAMBER_TEMP_PERIOD` | 120 | 300 |
| `WATCH_CHAMBER_TEMP_INCREASE` | 2 | 1 |
| `CHAMBER_FAN` | off | on, idx 0, mode 0, base 200 |

`Configuration_adv.h` is otherwise the stock Creality V4.2.2 example with the
array sizes cut to one axis, `BABYSTEPPING` off, `EMERGENCY_PARSER` and
`AUTO_REPORT_REDUNDANT` on.

## Known-open items

- `TEMP_SENSOR_REDUNDANT_MAX_DIFF 80` is a loose differential placeholder, not
  TEC protection. Tighten to `gap + 25` after measuring hot plate vs chamber air
  at 26 °C.
- `MAX_CHAMBER_POWER 64` is a first guess. Halve it if the chamber overshoots.
- PID gains are Marlin's 200 W SSR example, not a Peltier tune. Autotune at an
  8 °C delta may fail to converge.
- Never compiled. No ARM toolchain or PlatformIO was available at handover.

## Files

```
Configuration.h
Configuration_adv.h
pins/pins_CHAMBER_V422_GD32.h
README.md
SHA256SUMS.txt
```
