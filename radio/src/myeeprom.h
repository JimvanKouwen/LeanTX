/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MYEEPROM_H_
#define _MYEEPROM_H_

#include "datastructs.h"
#include "bitfield.h"
#include "hal/switch_driver.h"

// stick config
#define STICK_CFG_INV_BITS             1

// pots config
#define POT_CFG_TYPE_BITS              3
#define POT_CFG_INV_BITS               1
#define POT_CFG_BITS                   (POT_CFG_TYPE_BITS + POT_CFG_INV_BITS)
#define POT_CFG_MASK                   ((1 << POT_CFG_BITS) - 1)
#define POT_CONFIG_POS(x)              (POT_CFG_BITS * (x))
#define POT_CONFIG_MASK(x)             (POT_CFG_MASK << POT_CONFIG_POS(x))
#define POT_CONFIG_DISABLE_MASK(x)     (~POT_CONFIG_MASK(x))

#define SW_CFG_BITS                    2
#define SW_CFG_MASK                    ((1 << SW_CFG_BITS) - 1)
#define SWITCH_CONFIG_MASK(x)          ((swconfig_t)SW_CFG_MASK << (SW_CFG_BITS * (x)))

#define SWITCH_EXISTS(x)              (g_model.getSwitchType(x) != SWITCH_NONE)
#define IS_CONFIG_3POS(x)             (g_model.getSwitchType(x) == SWITCH_3POS)
#define IS_CONFIG_TOGGLE(x)           (g_model.getSwitchType(x) == SWITCH_TOGGLE)
#define SWITCH_WARNING_ALLOWED(x)     (SWITCH_EXISTS(x) && !IS_CONFIG_TOGGLE(x))

#define ALTERNATE_VIEW                0x10

#define SWITCHES_DELAY()            uint8_t(15+g_eeGeneral.switchesDelay)
#define SWITCHES_DELAY_NONE         (-15)
#define HAPTIC_STRENGTH()           (3+g_eeGeneral.hapticStrength)

enum CurveRefType {
  CURVE_REF_DIFF,
  CURVE_REF_EXPO,
  CURVE_REF_FUNC,
  CURVE_REF_CUSTOM
};

#define EXPO_VALID(ed)          ((ed)->srcRaw != 0)

#define limit_min_max_t     int16_t
#define LIMIT_EXT_PERCENT   150
#define LIMIT_STD_PERCENT   100
#define LIMIT_EXT_MAX       (LIMIT_EXT_PERCENT*10)
#define LIMIT_STD_MAX       (LIMIT_STD_PERCENT*10)
#define PPM_CENTER_MAX      500
#define LIMIT_MAX(lim) ((lim)->max + LIMIT_STD_MAX)
#define LIMIT_MIN(lim) ((lim)->min - LIMIT_STD_MAX)
#define LIMIT_OFS(lim) ((lim)->offset)
#define LIMIT_MAX_RESX(lim) calc1000toRESX(LIMIT_MAX(lim))
#define LIMIT_MIN_RESX(lim) calc1000toRESX(LIMIT_MIN(lim))
#define LIMIT_OFS_RESX(lim) calc1000toRESX(LIMIT_OFS(lim))

#define LIMITS_MIN_MAX_OFFSET LIMIT_STD_MAX

enum TelemetrySensorType
{
  TELEM_TYPE_CUSTOM,
  TELEM_TYPE_CALCULATED
};

enum TelemetrySensorFormula
{
  TELEM_FORMULA_ADD,
  TELEM_FORMULA_AVERAGE,
  TELEM_FORMULA_MIN,
  TELEM_FORMULA_MAX,
  TELEM_FORMULA_MULTIPLY,
  TELEM_FORMULA_TOTALIZE,
  TELEM_FORMULA_CELL,
  TELEM_FORMULA_CONSUMPTION,
  TELEM_FORMULA_DIST,
  TELEM_FORMULA_LAST = TELEM_FORMULA_DIST
};

#define IS_MANUAL_RESET_TIMER(idx)     (g_model.timers[idx].persistent == 2)

#define TIMER_COUNTDOWN_START(x)       (g_model.timers[x].countdownStart == 0 ? 20 : (g_model.timers[x].countdownStart == 1 ? 30 : (g_model.timers[x].countdownStart == -1 ? 10 : 5)))

#include "pulses/modules_constants.h"

#if !defined(BOOT)
extern RadioData g_eeGeneral;
extern ModelData g_model;
#endif

constexpr uint8_t EE_GENERAL = 0x01;
constexpr uint8_t EE_MODEL = 0x02;
constexpr uint8_t EE_LABELS = 0x04;

#endif // _MYEEPROM_H_
