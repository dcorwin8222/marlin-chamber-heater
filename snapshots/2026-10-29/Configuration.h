/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2025 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

/**
 * Heated chamber controller - GD32F303RET6 (Creality V4.2.2 pinout)
 * ==================================================================
 * Connectors used:
 *   HEATER1      PA1   TEC drive (hot face inside the chamber)
 *   FAN          PA0   stir fan
 *   TB1          PC4   chamber air NTC
 *   TH1          PC5   TEC hot-plate NTC (redundant sensor)
 *   EXP3-03/04   PB10/PB11  host UART (ESP32)
 *
 * Nothing else on the board is used. No hotend, no bed, no extruder,
 * no SD card, no display, no stepper drivers, no endstops, no probe.
 *
 * The TEC's cold face is outside the chamber. Its exterior fan runs
 * continuously and is not connected to this board.
 *
 * G-code:
 *   M141 S26        set chamber target (26C = 78.8F, mid-band)
 *   M191 S26        set chamber target and wait
 *   M105            C = chamber air,  M105 R adds R = TEC hot plate
 *   M303 E-2 S26 C8 autotune the chamber PID
 *   M112            emergency stop
 */

//===========================================================================
//============================= Version =====================================
//===========================================================================

#define CONFIGURATION_H_VERSION 02010300
#define STRING_CONFIG_H_AUTHOR "(chamber heater controller)"

//===========================================================================
//============================= Serial Port =================================
//===========================================================================

/**
 * GD32_MFL HAL serial index is 0-based:
 *   0 = USART0 (PA9/PA10, onboard CH340)   1 = USART1 (PA2/PA3)
 *   2 = USART2 (PB10/PB11)  <- EXP3, the host port
 *   3 = UART3 (PC10/PC11)                  4 = UART4 (PC12/PD2)
 */
#define SERIAL_PORT 2
#define BAUDRATE 250000

//===========================================================================
//============================= Machine =====================================
//===========================================================================

#ifndef MOTHERBOARD
  #define MOTHERBOARD BOARD_CHAMBER_V422_GD32
#endif

#define CUSTOM_MACHINE_NAME "Chamber Heater"
#define MACHINE_UUID "00000000-0000-0000-0000-000000000000"

//===========================================================================
//============================= Extruder ====================================
//===========================================================================

/**
 * No extruder. This is what makes HOTENDS resolve to 0, which in turn causes
 * Marlin to compile out PIDTEMP, PREVENT_COLD_EXTRUSION and
 * THERMAL_PROTECTION_HOTENDS entirely (Conditionals-1-axes.h).
 */
#define EXTRUDERS 0

/**
 * Still required by the source even with no extruder (pulled in by M200 and
 * the settings code). Value is unused.
 */
#define DEFAULT_NOMINAL_FILAMENT_DIA 1.75

//===========================================================================
//============================= Temperature =================================
//===========================================================================

/**
 * Sensor map. Only three slots are real:
 *
 *   TEMP_SENSOR_CHAMBER  -> TEMP_CHAMBER_PIN (PC4), drives the heater
 *   TEMP_SENSOR_REDUNDANT-> reuses TEMP_1_PIN (PC5) as its own input
 *   TEMP_SENSOR_0        -> must stay 0, there is no hotend
 *
 * "1" is the 100K EPCOS table. Change the number here if your probes differ;
 * the redundant sensor takes its table from TEMP_SENSOR_REDUNDANT, not from
 * TEMP_SENSOR_1.
 */
#define TEMP_SENSOR_0 0
#define TEMP_SENSOR_1 0
#define TEMP_SENSOR_BED 0
#define TEMP_SENSOR_CHAMBER 1
#define TEMP_SENSOR_PROBE 0
#define TEMP_SENSOR_COOLER 0
#define TEMP_SENSOR_BOARD 0
#define TEMP_SENSOR_SOC 0
#define TEMP_SENSOR_REDUNDANT 1

/**
 * Redundant sensor: PC5 watches PC4 (temperature.cpp:2379).
 *
 * PC5 is bonded to the TEC hot plate; PC4 is in the chamber air. The plate
 * runs far hotter than the air, so this is a differential plausibility check,
 * NOT an absolute limit for the TEC - it cannot stop the plate at a rated
 * maximum, only notice that the two probes disagree.
 *
 * 80 is a deliberately loose starting guess: it will not fire until the plate
 * is roughly 80C above the chamber air, i.e. around 106C at a 26C chamber.
 * Measure the real plate/air gap at working temperature and tighten this to
 * (gap + 25C). See README.
 */
#if TEMP_SENSOR_REDUNDANT
  #define TEMP_SENSOR_REDUNDANT_SOURCE    E1
  #define TEMP_SENSOR_REDUNDANT_TARGET    CHAMBER
  #define TEMP_SENSOR_REDUNDANT_MAX_DIFF  80
#endif

/**
 * Chamber limits, in Celsius. Marlin has no Fahrenheit support anywhere and
 * the setpoint quantises to whole degrees:
 *
 *   24C = 75.2F   bottom of the sample's ideal band
 *   27C = 80.6F   top of the ideal band, and the highest target settable
 *                 (CHAMBER_MAXTEMP - CHAMBER_OVERSHOOT)
 *   32C = 89.6F   hard trip: heater off, needs M999
 *   38C = 100.4F  sample dies above here, hence the 6C of margin
 *
 * CHAMBER_MAXTEMP is three things at once: the MAXTEMP trip, the upper gate
 * on heater output, and the top of the chamber thermistor's ADC window.
 * Lowering it tightens all three together.
 */
#define CHAMBER_MINTEMP              5  // (C) 41F, heater never runs below this
#define CHAMBER_MAXTEMP             32  // (C) 89.6F trip
#define CHAMBER_OVERSHOOT            5  // (C) caps the target at 27C / 80.6F

/** M191 settle detection. No defaults exist in Marlin for these. */
#define TEMP_CHAMBER_HYSTERESIS      1  // (C) stop heating within 1C of target
#define TEMP_CHAMBER_WINDOW          1  // (C) proximity for the "reached" timer
#define TEMP_CHAMBER_RESIDENCY_TIME 10  // (s) how long it must stay there

//===========================================================================
//============================= Chamber PID =================================
//===========================================================================

/**
 * PID on the TEC. The gains below are Marlin's published example for a 200W
 * resistive element through an SSR - the closest documented starting point,
 * not a tune for a Peltier. A TEC is worse behaved: its plate stores heat, so
 * cutting drive does not stop heat entering the chamber. Treat
 * MAX_CHAMBER_POWER as the real control and see README.
 *
 * M303 E-2 prints its result as CHAMBER_KP; with no EEPROM it has to be pasted
 * back in here and reflashed. At an 8C delta above ambient autotune often fails
 * outright - do not count on it.
 */
#define PIDTEMPCHAMBER
#define MAX_CHAMBER_POWER  64   // (of 255) 25% duty; halve it if it overshoots
#define MIN_CHAMBER_POWER   0
#define PID_K1           0.95   // Smoothing factor within the PID loop

#if ENABLED(PIDTEMPCHAMBER)
  #define DEFAULT_CHAMBER_KP  37.04
  #define DEFAULT_CHAMBER_KI   1.40
  #define DEFAULT_CHAMBER_KD 655.17
#endif

/**
 * PID_FUNCTIONAL_RANGE has no default in the source. Outside this band the
 * loop is bypassed (0% above target, 100% well below it) and the integrator is
 * reset, so it also sets how hard the chamber heater can initially drive.
 */
#define PID_FUNCTIONAL_RANGE 20  // (C)

//===========================================================================
//========================= Thermal Protection ==============================
//===========================================================================

/**
 * Runaway detection for the chamber heater. The time windows live in
 * Configuration_adv.h (THERMAL_PROTECTION_CHAMBER_PERIOD / WATCH_CHAMBER_*)
 * and are widened there, because chamber air heats far too slowly for the
 * stock values.
 */
#define THERMAL_PROTECTION_CHAMBER

//===========================================================================
//============================= Axes ========================================
//===========================================================================

/**
 * Marlin cannot build with zero axes: SanityCheck compares
 * DEFAULT_AXIS_STEPS_PER_UNIT against DISTINCT_AXES and indexes into it
 * unconditionally (sanity.h:3911), so an empty axis count is a hard error.
 *
 * X is therefore declared and never used. One driver type, one set of pins,
 * one element per array. The pins are the board's real X pins (PC2/PB9/PC3);
 * nothing is wired to them and no G-code ever moves the axis. This is the
 * smallest axis set Marlin accepts, not a machine feature.
 */
#define X_DRIVER_TYPE A4988

#define DEFAULT_AXIS_STEPS_PER_UNIT   { 80 }
#define DEFAULT_MAX_FEEDRATE          { 300 }
#define DEFAULT_MAX_ACCELERATION      { 500 }

#define DEFAULT_ACCELERATION          500
#define DEFAULT_TRAVEL_ACCELERATION   500
#define DEFAULT_RETRACT_ACCELERATION  500

#define CLASSIC_JERK
#if ENABLED(CLASSIC_JERK)
  #define DEFAULT_XJERK 10.0
#endif

// Y_BED_SIZE is required even though the Y axis does not exist (sanity.h:258).
#define X_BED_SIZE 200
#define Y_BED_SIZE 200

#define X_MIN_POS 0
#define X_MAX_POS X_BED_SIZE

#define X_HOME_DIR -1
#define INVERT_X_DIR false
#define X_ENABLE_ON 0

#define HOMING_FEEDRATE_MM_M { 50 }

/**
 * X_HOME_DIR -1 makes the pins postprocessor alias the board's X_STOP_PIN
 * (PA5) to X_MIN_PIN, which sets USE_X_MIN and compiles the endstop reader
 * that wants this macro. Nothing is plugged into PA5; it is never homed.
 */
#define X_MIN_ENDSTOP_HIT_STATE HIGH

// No endstops are fitted, so no USE_XMIN_PLUG / USE_XMAX_PLUG.

//===========================================================================
//============================= EEPROM ======================================
//===========================================================================

/**
 * Deliberately no EEPROM_SETTINGS.
 *
 * The GD32_MFL HAL has no internal-flash EEPROM. Its only fallbacks are
 * SDCARD_EEPROM_EMULATION (needs an SD card and SDSUPPORT) or an external
 * I2C/SPI chip - neither is fitted. Enabling EEPROM_SETTINGS without one is a
 * hard build error in HAL/GD32_MFL/inc/SanityCheck.h.
 *
 * Consequence: M500/M501/M503 do not exist, and anything tuned at runtime is
 * lost on power-off. PID gains from M303 must be written into this file and
 * the firmware reflashed.
 */

//===========================================================================
//============================= Nothing else ================================
//===========================================================================

/**
 * Deliberately absent: SDSUPPORT, any display, BLTouch/probe, servo,
 * filament runout, bed, hotend, second extruder, TMC drivers, endstops,
 * power supply control, filament width sensor, mixing extruder, MMU.
 *
 * The stir fan is FAN0 (PA0) and is driven by Marlin's CHAMBER_FAN option in
 * Configuration_adv.h: it runs whenever a chamber target is set, and stops
 * when the target is 0. M106/M107 do not control it.
 */
