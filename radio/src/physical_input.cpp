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

#include "edgetx.h"
#include "physical_input.h"
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"

static const int16_t _switch_2pos_lookup[] = {-RESX, RESX, RESX};
static const int16_t _switch_3pos_lookup[] = {-RESX, 0, RESX};

bool isPhysicalInputAvailable(PhysicalInputId source)
{
  unsigned id = unsigned(source);
  if (id >= 1 && id < 33) return id - 1 < adcGetMaxInputs(ADC_INPUT_MAIN);
  if (id >= 33 && id < 65)
    return id - 33 < adcGetMaxInputs(ADC_INPUT_FLEX) && IS_POT_SLIDER_AVAILABLE(id - 33);
  if (id >= 65 && id < 97 && id - 65 < switchGetMaxAllSwitches()) {
    auto type = g_model.getSwitchType(id - 65);
    return type == SWITCH_TOGGLE || type == SWITCH_2POS || type == SWITCH_3POS;
  }
  return false;
}

int16_t readPhysicalInput(PhysicalInputId source)
{
  if (!isPhysicalInputAvailable(source)) return 0;
  unsigned id = unsigned(source);
  if (id < 33) return calibratedAnalogs[inputMappingConvertMode(id - 1)];
  if (id < 65) return calibratedAnalogs[adcGetInputOffset(ADC_INPUT_FLEX) + id - 33];
  {
    auto sw_idx = uint8_t(id - 65);
    // Physical inputs always read hardware, including function buttons.
    auto sw_cfg = g_model.getSwitchType(sw_idx);
    switch(sw_cfg) {
    default:
      return 0;
    case SWITCH_TOGGLE:
    case SWITCH_2POS:
      return _switch_2pos_lookup[switchGetPosition(sw_idx)];
    case SWITCH_3POS:
      return _switch_3pos_lookup[switchGetPosition(sw_idx)];
    }
  }
}

BeepANACenter bpanaCenter = 0;
int16_t calibratedAnalogs[MAX_ANALOG_INPUTS];

void evalAnalogControls(bool beep)
{
  BeepANACenter anaCenter = 0;

#if defined(STICK_DEAD_ZONE)
  int16_t deadZoneOffset = g_eeGeneral.stickDeadZone ? 2 << (g_eeGeneral.stickDeadZone - 1) : 0;
#endif

  auto max_calib_analogs = adcGetInputOffset(ADC_INPUT_VBAT);
  auto pots_offset = adcGetInputOffset(ADC_INPUT_FLEX);

  for (uint8_t i = 0; i < max_calib_analogs; i++) {
    int16_t v = anaIn(i);
    uint8_t ch = (i < pots_offset ? inputMappingConvertMode(i) : i);

    // [0..2048] -> [-1024..1024]
    v -= RESX;

#if defined(STICK_DEAD_ZONE)
    // dead zone invented by FlySky in my opinion it should goes into ADC
    if (g_eeGeneral.stickDeadZone && ch != inputMappingGetThrottle()) {
      if (v > deadZoneOffset) {
        // y=ax+b
        v = (int16_t)((int32_t)(v - deadZoneOffset) * 1024L / (1024L - deadZoneOffset));
      } else if (v < -deadZoneOffset) {
        // y=ax+b
        v = (int16_t)((int32_t)(v + deadZoneOffset) * 1024L / (1024L - deadZoneOffset));
      } else {
        v = 0;
      }
    }
#endif

    BeepANACenter mask = (BeepANACenter)1 << ch; // TODO

    calibratedAnalogs[i] = v;

    // filtering for center beep
    uint8_t tmp = (uint16_t)abs(v) / 16;
    if (beep) {
      if (tmp==0 || (tmp==1 && (bpanaCenter & mask))) {
        anaCenter |= mask;
        if ((g_model.beepANACenter & mask) && !(bpanaCenter & mask) &&
            controlsInitialized && !menuCalibrationState) {
          if (i < pots_offset || IS_POT_SLIDER_AVAILABLE(i - pots_offset)) {
            AUDIO_POT_MIDDLE(i);
          }
        }
      }
    }

  }

  if (beep) {
    bpanaCenter = anaCenter;
  }
}


bool isPhysicalSwitchConditionAvailable(int condition)
{
  if (condition == 0) return true;
  if (condition < 1 || condition > PHYSICAL_SWITCH_CONDITION_MAX) return false;
  unsigned index = (condition - 1) / 3;
  unsigned position = (condition - 1) % 3;
  if (!isPhysicalInputAvailable(physicalSwitch(index))) return false;
  if (switchIsFlex(index) && !switchIsFlexValid(index)) return false;
#if defined(FUNCTION_SWITCHES)
  if (switchIsCustomSwitch(index)) return position != 1;
#endif
  return position != 1 || g_model.getSwitchType(index) == SWITCH_3POS;
}

bool readPhysicalSwitchCondition(uint8_t condition)
{
  if (!condition || !isPhysicalSwitchConditionAvailable(condition)) return false;
  unsigned index = (condition - 1) / 3;
  int position = (condition - 1) % 3;
  return readPhysicalInput(physicalSwitch(index)) == (position - 1) * RESX;
}
