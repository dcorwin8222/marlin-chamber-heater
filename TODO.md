# Host controller TODO — ESP32 / ESPHome

Planning notes only. Nothing here is implemented. The Marlin side is finished and
frozen; this is the host that talks to it at 115200 on `EXP3-03/04`.

Related: `README.md` §1 (wiring) and §10 (first run) describe the Marlin side as it
stands today, without a relay.

---

## Open decisions

- [ ] **On WiFi/HA loss: hold the setpoint, or drop to 0?** Holding protects the
      sample; dropping fails cold. Pick one and make it explicit in the config.
- [ ] **Board:** reuse the `esp32dev` with GPIO16/17, or a dedicated ESP32 with
      spare GPIO for relay, buzzer and LED? Any pin can carry the UART, so this is
      really about how many spare GPIO you want.

---

## Hardware — relay

Decided: the relay is a **series master cutoff between the 4.2.2 and the TEC**.
Marlin keeps doing the modulation on the board's own MOSFET; the relay is not in
the control loop.

- [ ] **Part:** Songle **SRD-12VDC-SL-C** (SPDT, sealed, ~19 × 15 × 15 mm,
      10 A @ 30 VDC, coil ~30 mA at 12 V). Usually cheapest as a "1-channel 12 V
      relay module" with opto, driver and flyback already fitted.
      Name-brand equal: Omron **G5LE-1** (8 A @ 30 VDC) or Panasonic **ALZ51B12**.
- [ ] **Do not** substitute a sub-miniature signal relay. Omron G6K, Panasonic
      TQ2 and Fujitsu FTR-B3 are 1–2 A @ 30 VDC — no margin on a continuous
      2.5 A load, on the one part that is the safety net.
- [ ] **Check the module's trigger works at 3.3 V.** Many "12 V relay modules"
      want a 5 V trigger. If yours does, put a 2N7000 or BC547 between the GPIO
      and the coil input, with a 1N4007 across the coil.
- [ ] **Never drive the coil from a GPIO directly** — ~30 mA continuous is past
      what an ESP32 pin should source.
- [ ] **Wire the high side**, not the switched lead:

      ```
      12V+ ──[3 A fuse]──[relay NO]── TEC +        TEC − ── HEATER1(−)
      ```

      Breaking the negative instead leaves 12 V on the module's `+` and floats
      the open contact to the rail. Worse isolation, no benefit.
- [ ] Coil fed from the **same 12 V rail as the TEC**, so a dead supply opens the
      contact too.
- [ ] 3 A fuse in the TEC branch: 2.5 A is the running load, a stalled module is a
      dead short.
- [ ] TVS across the TEC (e.g. SMBJ16A) or a reverse diode: the module kicks at
      7.6 Hz and the board's FET absorbs it otherwise.
- [ ] Use **NO**, coil energised = heating permitted. That is the dead-man: ESP32
      dies → coil drops → TEC isolated even with PA1's MOSFET shorted.
- [ ] 18 AWG in the TEC branch; the module's screw terminals take 2.5 A fine.
- [ ] With the case open, glance at the `HEATER1` FET package. It is sized for a
      ~40 W hotend at 24 V, so 30 W at 12 V is inside its rating, but it now
      carries 2.5 A continuously.

---

## Hardware — integration rules

- [ ] ESP32 opens the relay on any fault, on ESP32 shutdown, and on link-watchdog
      expiry.
- [ ] **Cut order:** `M141 S0` → open relay. `M112` only when the link is dead and
      a kill is required. `M112` is caught by `EMERGENCY_PARSER` mid-buffer;
      `M141 S0` is not, so it must not be the last line of defence on a sick link.
- [ ] **Close the relay only with the target at 0**, then send the target. Avoids a
      hot make into a module that the board is already switching at 7.6 Hz.
- [ ] Expect `Heating failed` from Marlin (after `WATCH_CHAMBER_TEMP_PERIOD`, 300 s)
      whenever the relay is opened while a target is still set and there is no link
      to clear it. That is the correct outcome; `M999` recovers.
- [ ] **Never let the relay carry the PWM.** `SOFT_PWM_SCALE 0` switches PA1 at
      7.63 Hz — ~27,000 operations an hour. `MAX_CHAMBER_POWER` does not slow it.
      If that ever has to change, `SLOW_PWM_HEATERS` drops it to an 8 s window
      (0.125 Hz) and the gains need retuning again.

---

## First-run additions (once the relay is fitted)

- [ ] 12 V rail switched off → relay drops out, TEC branch reads 0 V. Repeat with
      the ESP32 powered down but 12 V present.
- [ ] Pull ESP32 power mid-heat → TEC voltage falls to 0 and Marlin reports
      `Heating failed` after 300 s. That is the dead-man proven end to end.
- [ ] With the relay open, `M141 S26` produces no heat and no current in the TEC
      branch.
- [ ] Confirm the relay only closes after `M141 S0`, and watch the first make.
- [ ] Confirm fuse rating on the actual branch current, not the nameplate.

---

## P0 — must work before it is allowed to heat anything

- [ ] **Serial layer.** Small custom component, not lambdas. Connect, resync on
      Marlin's `start` banner after either side reboots, queue until `ok`, tolerate
      `echo:`, `ok`, `busy: processing`, `!!`. Done when the Creality board can be
      power-cycled and the link recovers untouched.
- [ ] **Stop path that cannot be blocked.** `M141 S0` on a healthy link, `M112`
      written raw past the queue when it is not.
- [ ] **Link watchdog.** No `ok` and no temperature frame for N seconds → fault,
      `M112`, relay open, HA notification. Done when pulling RX produces a fault
      rather than silence.
- [ ] **Independent over-temperature guard.** Own thresholds for `C:` and `R:`,
      latching, cuts output. Done when a simulated stuck heater is stopped with
      Marlin's own limits disabled.
- [ ] **Fault vocabulary.** Recognise `MAXTEMP`, `Heating failed`,
      `Redundant temp sensor`, `KILLED`, `Error:`. One enum sensor plus a
      `Clear fault` button sending `M999`.

---

## P1 — the daily driver

- [ ] **Target** number entity → `M141 Sn`, clamped 24–27, read the applied target
      back from the report rather than assuming.
- [ ] **Sensors** from `M155` auto-report: chamber `C:`, plate `R:`, duty `C@:`.
      `AUTO_REPORT_TEMPERATURES` and `AUTO_REPORT_REDUNDANT` are already on in the
      Marlin build, so this is listen-only. Update ≤ 2 s.
- [ ] **Enable/disable switch**, with an explicit answer to "does enable restore the
      last target or go to 24".
- [ ] **Status enum:** offline / idle / heating / holding / fault.
- [ ] **HA dashboard + fault notification.** Nothing fancier.

---

## P2 — trust it unattended

- [ ] **Reboot policy.** Restore the target across an ESP32 restart, but never
      after a fault or a kill.
- [ ] **HA/WiFi-loss policy** — the open decision above, implemented and tested.
- [ ] **Ramp assist.** Step the target up (1 °C per few minutes) instead of `S26`
      from ambient. Cheapest real fix for TEC overshoot, no Marlin change.
- [ ] **Run log.** Time in band, min/max, excursions, worst `R: − C:` gap. Feeds the
      `TEMP_SENSOR_REDUNDANT_MAX_DIFF` value in `README.md` §6.5.
- [ ] **Gains workflow.** Buttons to dump `M503` and store the `M309` line, so a
      tune is recoverable without a terminal.

---

## P3 — convenience

- [ ] Schedules and saved profiles (overnight hold at 24).
- [ ] Local LED/buzzer for fault, so the room reports without HA.
- [ ] Raw serial passthrough for debugging.
- [ ] OTA, uptime, RSSI.
- [ ] Exterior fan control — **only** if that fan ever moves onto the ESP32. It
      currently runs continuously off-board; leave it.

---

## Not building

- No PID on the ESP32. Marlin owns the loop; two controllers on one heater
  oscillate.
- No GPIO drive of the TEC. One authority for the heater, and it is the relay.
- No auto-retry after a thermal fault. Every kill needs a human.
