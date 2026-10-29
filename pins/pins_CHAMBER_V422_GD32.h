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
 * Creality V4.2.2 (GD32F303RET6) used only as a heated-chamber controller.
 *
 * Same pin map as gd32f3/pins_CREALITY_V422_GD32_MFL.h, which is only a
 * two-line overlay (ALLOW_GD32F3 + include) on the STM32F1 file, so the
 * STM32F1 file is included directly here. Every peripheral this machine does
 * not have is removed afterwards. What survives:
 *
 *   PA1   TEC drive           HEATER_CHAMBER_PIN (set in Configuration_adv.h)
 *   PA0   stir fan            FAN0_PIN
 *   PC4   chamber air NTC     TEMP_CHAMBER_PIN   (TB1 connector)
 *   PC5   TEC hot-plate NTC   TEMP_1_PIN         (TH1 connector)
 *   PB10/PB11   host UART     SERIAL_PORT 2      (EXP3-03/04)
 *   PC2/PB9/PC3 X stepper     unused, see Configuration.h
 */

#define ALLOW_GD32F3

#define BOARD_INFO_NAME      "Chamber Heater V4.2.2 GD32"
#define DEFAULT_MACHINE_NAME "Chamber Heater"

#include "../stm32f1/pins_CREALITY_V4.h"

/**
 * The two sensors that exist. PC4 and PC5 are both 12-bit ADC inputs
 * (ADC12_IN14 / ADC12_IN15) and do not contend.
 *
 * The chamber probe is plugged into TB1, the bed thermistor connector. The TEC
 * hot-plate probe is plugged into TH1. Both are the same 100K NTC type; Marlin
 * does not care about the silkscreen, only which pin each function reads.
 */
#define TEMP_CHAMBER_PIN                    PC4
#define TEMP_1_PIN                          PC5

/**
 * Everything below is present on a stock V4.2.2 and would otherwise be
 * initialised as a live output. Removing them keeps a mistyped G-code from
 * driving a FET that has nothing (or something unexpected) attached.
 */
#undef TEMP_0_PIN
#define TEMP_0_PIN                          -1    // no hotend
#undef TEMP_BED_PIN
#define TEMP_BED_PIN                        -1    // no bed
#undef HEATER_0_PIN
#define HEATER_0_PIN                        -1    // PA1 belongs to the chamber
#undef HEATER_BED_PIN
#define HEATER_BED_PIN                      -1    // no bed

/**
 * X stepper pins (PC2 / PB9 / PC3) are inherited and unused. Marlin requires
 * at least one axis to compile; see the Axes section of Configuration.h.
 */
