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

// Generic UI/audio/Lua values. Not part of realtime control routing.
#include "edgetx.h"
#include "physical_input.h"
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"
#if defined(LUMINOSITY_SENSOR)
#include "luminosity_sensor.h"
#endif

// TODO same naming convention than the drawSource
// *valid added to return status to Lua for invalid sources
static getvalue_t _getValue(mixsrc_t i, bool* valid)
{
  if (i <= MIXSRC_NONE || i > MIXSRC_LAST_TELEM) {
    if (valid != nullptr) *valid = false;
    return 0;
  }

  else if (i <= MIXSRC_LAST_STICK) {
    auto source = physicalStick(i - MIXSRC_FIRST_STICK);
    if (valid && !isPhysicalInputAvailable(source)) *valid = false;
    return readPhysicalInput(source);
  }
  else if (i <= MIXSRC_LAST_POT) {
    auto source = physicalFlex(i - MIXSRC_FIRST_POT);
    if (valid && !isPhysicalInputAvailable(source)) *valid = false;
    return readPhysicalInput(source);
  }

#if defined(IMU)
  else if (i == MIXSRC_TILT_X) {
    return gyroScaledX();
  }
  else if (i == MIXSRC_TILT_Y) {
    return gyroScaledY();
  }
#endif

#if defined(PCBHORUS)
  else if (i >= MIXSRC_FIRST_SPACEMOUSE && i <= MIXSRC_LAST_SPACEMOUSE) {
#if defined(SPACEMOUSE)
    return get_spacemouse_value(i - MIXSRC_FIRST_SPACEMOUSE);
#else
    return 0;
#endif
  }
#endif

#if defined(LUMINOSITY_SENSOR)
  else if (i == MIXSRC_LIGHT) {
    return getLuxSensorValue() - RESX;
  }
#endif

  else if (i < MIXSRC_FIRST_SWITCH) {
    if (valid != nullptr) *valid = false;
    return 0; // Reserved source IDs from the removed trim-value system.
  }
  else if (i >= MIXSRC_FIRST_SWITCH && i <= MIXSRC_LAST_SWITCH) {
    auto source = physicalSwitch(i - MIXSRC_FIRST_SWITCH);
    if (valid && !isPhysicalInputAvailable(source)) *valid = false;
    return readPhysicalInput(source);
  }
#if defined(FUNCTION_SWITCHES)
    else if (i <= MIXSRC_LAST_CUSTOMSWITCH_GROUP) {
      uint8_t group_idx = (uint8_t)(i - MIXSRC_FIRST_CUSTOMSWITCH_GROUP + 1);
      uint8_t stepcount = getSwitchCountInFSGroup(group_idx);
      if (stepcount == 0)
        return 0;

      if (g_model.cfsGroupAlwaysOn(group_idx))
        stepcount--;

      // An always-on singleton has only one possible position.
      if (stepcount == 0) return 0;

      int stepsize = (2 * RESX) / stepcount;
      int value = -RESX;

      for (uint8_t i =  0; i < switchGetMaxSwitches(); i++) {
        if (switchIsCustomSwitch(i)) {
          if (g_model.getSwitchGroup(i) == group_idx) {
            if (g_model.cfsState(i) == 1)
              return value + (g_model.cfsGroupAlwaysOn(group_idx) ? 0 : stepsize);
            else
              value += stepsize;
          }
        }
      }
      return -RESX;
  }
#endif

  else if (i == MIXSRC_TX_VOLTAGE) {
    return g_vbat100mV;
  } else if (i == MIXSRC_TX_GPS) {
    // GPS is structured data, exposed through the dedicated GPS API.
    if (valid != nullptr) *valid = false;
    return 0;
  } else if (i == MIXSRC_TX_TIME) {
#if defined(RTCLOCK)
    return (g_rtcTime % SECS_PER_DAY) / 60; // number of minutes from midnight
#else
    if (valid != nullptr) *valid = false;
    return 0;
#endif
  } else if (i <= MIXSRC_LAST_TIMER) {
    return timersStates[i - MIXSRC_FIRST_TIMER].val;
  }

  // Telemetry remains available to UI/audio/Lua.
  else if (i <= MIXSRC_LAST_TELEM) {
    if (IS_FAI_FORBIDDEN(i)) {
      if (valid != nullptr) *valid = false;
      return 0;
    }
    i -= MIXSRC_FIRST_TELEM;
    div_t qr = div((uint16_t)i, 3);
    TelemetryItem & telemetryItem = telemetryItems[qr.quot];
    switch (qr.rem) {
      case 1:
        return telemetryItem.valueMin;
      case 2:
        return telemetryItem.valueMax;
      default:
        return telemetryItem.value;
    }
  }

  if (valid != nullptr) *valid = false;
  return 0;
}

getvalue_t getValue(mixsrc_t i, bool* valid)
{
  if (i < -MIXSRC_LAST_TELEM || i > MIXSRC_LAST_TELEM) {
    if (valid) *valid = false;
    return 0;
  }
  bool invert = false;
  if (i < 0) {
    invert = true;
    i = -i;
  }
  getvalue_t v = _getValue(i, valid);
  if (invert) v = -v;
  return v;
}

