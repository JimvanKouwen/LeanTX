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

#include "hal/switch_driver.h"
#include "hal/adc_driver.h"
#include "hal/rgbleds.h"

#include "myeeprom.h"
#include "edgetx.h"
#include "edgetx_constants.h"
#include "os/sleep.h"
#include "switches.h"
#include "input_mapping.h"
#include "inactivity_timer.h"
#include "tasks/mixer_task.h"

#if defined(RADIO_GX12)
#include "targets/taranis/gx12/bsp_io.h"
#endif

#if defined(COLORLCD)
  #define SWITCH_WARNING_LIST_X        WARNING_LINE_X
  #define SWITCH_WARNING_LIST_Y        WARNING_LINE_Y+3*FH
#elif LCD_W >= 212
  #define SWITCH_WARNING_LIST_X        60
  #define SWITCH_WARNING_LIST_Y        4*FH+4
#else
  #define SWITCH_WARNING_LIST_X        4
  #define SWITCH_WARNING_LIST_Y        4*FH+4
#endif

tmr10ms_t switchesMidposStart[MAX_SWITCHES];
uint64_t  switchesPos = 0;

static_assert(sizeof(uint64_t) * 8 >= ((MAX_SWITCHES - 1) / 2) + 1,
              "MAX_SWITCHES too big for uint64_t position state");

tmr10ms_t potsLastposStart[MAX_POTS];
uint8_t   potsPos[MAX_POTS];

#define SWITCH_POSITION(sw) (switchesPos & ((MASK_CFN_TYPE)1 << (sw)))
#define POT_POSITION(sw)                            \
  ((potsPos[(sw) / XPOTS_MULTIPOS_COUNT] & 0x0f) == \
   ((sw) % XPOTS_MULTIPOS_COUNT))

#if defined(FUNCTION_SWITCHES)
// Customizable switches
//
// Non pushed : SWSRC_Sx0 = -1024 = Sx(up) = state 0
// Pushed : SWSRC_Sx2 = +1024 = Sx(down) = state 1

uint32_t fsPreviousState = 0;

uint8_t isSwitch3Pos(uint8_t idx)
{
  return IS_CONFIG_3POS(idx);
}

#if defined(SIMU)
bool evalFSok = false;
#endif

void setFSStartupPosition()
{
  for (uint8_t i = 0; i < switchGetMaxSwitches(); i++) {
    if (switchIsCustomSwitch(i)) {
      uint8_t startPos = g_model.getSwitchStart(i);
      if (g_model.getSwitchType(i) == SWITCH_TOGGLE)
        startPos = FS_START_OFF;
      switch(startPos) {
        case FS_START_OFF:
          g_model.cfsSetState(i, 0);
          break;

        case FS_START_ON:
          g_model.cfsSetState(i, 1);
          break;

        case FS_START_PREVIOUS:
        default:
          // Do nothing, use existing value
          break;
      }
    }
  }

#if defined(SIMU)
  evalFSok = true;
#endif
}

void setFSLogicalState(uint8_t index, uint8_t value)
{
  g_model.cfsSetState(index, value ? 1 : 0);
}

bool getFSPhysicalState(uint8_t index)
{
#if defined(FUNCTION_SWITCHES)
  if (switchIsCustomSwitch(index) && g_model.cfsSFState(index))
    return true;
#endif

  return switchGetPosition(index) != SWITCH_HW_UP;
}

static bool getFSPreviousPhysicalState(uint8_t index)
{
  return (fsPreviousState >> index) & 1;
}

uint8_t getSwitchCountInFSGroup(uint8_t index)
{
  uint8_t count = 0;

  for (uint8_t i = 0; i < switchGetMaxSwitches(); i++) {
    if (switchIsCustomSwitch(i) && g_model.getSwitchGroup(i) == index)
      count++;
  }

  return count;
}

bool isFSGroupUsed(uint8_t index)
{
  return getSwitchCountInFSGroup(index) != 0;
}

void evalFunctionSwitches()
{
#if defined(SIMU)
  if (!evalFSok) return;
#endif

  for (uint8_t i = 0; i < switchGetMaxSwitches(); i++) {
    if (switchIsCustomSwitch(i)) {
      if (g_model.getSwitchType(i) == SWITCH_NONE) {
        continue;
      }

      bool physicalState = getFSPhysicalState(i);
      if (physicalState != getFSPreviousPhysicalState(i)) {
        // FS was moved
        inactivityTimerReset(ActivitySource::MainControls);
        if ((g_model.getSwitchType(i) == SWITCH_2POS && physicalState) ||
            (g_model.getSwitchType(i) == SWITCH_TOGGLE)) {
          if (g_model.cfsGroupAlwaysOn(g_model.getSwitchGroup(i)) != 0) {
            // In an always on group
            g_model.cfsSetState(i, 1);
          } else {
            g_model.cfsSetState(i, g_model.cfsState(i) ^ 1); // Toggle bit
          }
        }

        if (g_model.getSwitchGroup(i) && physicalState) {
          // switch is in a group, other in group need to be turned off
          for (uint8_t j = 0; j < switchGetMaxSwitches(); j++) {
            if ((i != j) && switchIsCustomSwitch(j)) {
              if (g_model.getSwitchGroup(j) == g_model.getSwitchGroup(i)) {
                g_model.cfsSetState(j, 0);
              }
            }
          }
        }

        fsPreviousState ^= uint32_t(1) << i;  // Toggle state
        storageDirty(EE_MODEL);
      }

      if (!pwrPressed()) {
        if (g_model.cfsState(i))
          setFSLedON(i);
        else
          setFSLedOFF(i);
      }
    }
  }
}

bool groupHasSwitchOn(uint8_t group)
{
  for (int j = 0; j < switchGetMaxSwitches(); j += 1)
    if (switchIsCustomSwitch(j) && g_model.getSwitchGroup(j) == group && g_model.cfsState(j))
      return true;
  return false;
}

int firstSwitchInGroup(uint8_t group)
{
  for (int j = 0; j < switchGetMaxSwitches(); j += 1)
    if (switchIsCustomSwitch(j) && g_model.getSwitchGroup(j) == group)
      return j;
  return -1;
}

int groupDefaultSwitch(uint8_t group)
{
  bool allOff = true;
  for (int j = 0; j < switchGetMaxSwitches(); j += 1) {
    if (switchIsCustomSwitch(j)) {
      if (g_model.getSwitchGroup(j) == group) {
        if (g_model.getSwitchStart(j) == FS_START_ON)
          return j;
        if (g_model.getSwitchStart(j) != FS_START_OFF)
          allOff = false;
      }
    }
  }
  if (allOff)
    return switchGetMaxSwitches();
  return -1;
}

void setGroupSwitchState(uint8_t group)
{
  // Check rules for always on group
  //  - Toggle switch type not valid, change all switches to 2POS
  //  - One switch must be turned on, turn on first switch if needed
  if (g_model.cfsGroupAlwaysOn(group)) {
    for (int j = 0; j < switchGetMaxSwitches(); j += 1) {
      if (switchIsCustomSwitch(j) && g_model.getSwitchGroup(j) == group) {
        g_model.setSwitchType(j, SWITCH_2POS); // Toggle not valid
      }
    }
    if (!groupHasSwitchOn(group)) {
      int sw = firstSwitchInGroup(group);
      if (sw >= 0)
        setFSLogicalState(sw, 1); // Make sure a switch is on
    }
    if (groupDefaultSwitch(group) == switchGetMaxSwitches()) {
      // Start state for all switches is off - set all to 'last'
      for (int j = 0; j < switchGetMaxSwitches(); j += 1)
        if (switchIsCustomSwitch(j) && g_model.getSwitchGroup(j) == group)
          g_model.setSwitchStart(j, FS_START_PREVIOUS);
    }
  }
}
#endif // FUNCTION_SWITCHES

div_t switchInfo(int switchPosition)
{
  return div(switchPosition - SWSRC_FIRST_SWITCH, 3);
}

int switchLookupIdx(char c)
{
  uint8_t idx = 1; // Sx
  if (c >= '1' && c <= '9') {
      idx = 2; // SWx
  }

  auto max_switches = switchGetMaxAllSwitches();
  for (int i = 0; i < max_switches; i++) {
    const char *name = switchGetDefaultName(i);
    if (name[idx] == c) return i;
  }

  return -1;
}

int switchLookupIdx(const char* name, size_t len)
{
  if (len < 2 || (name[0] != 'S' && name[0] != 'F')) return -1;

  auto max_switches = switchGetMaxAllSwitches();
  for (int i = 0; i < max_switches; i++) {
    const char *sw_name = switchGetDefaultName(i);
    if (strlen(sw_name) == len && strncmp(sw_name, name, len) == 0) return i;
  }

  return -1;
}

char switchGetLetter(uint8_t idx)
{
  if (idx >= switchGetMaxAllSwitches() + MAX_FLEX_SWITCHES)
    return -1;

  const char* name = switchGetDefaultName(idx);
  if (!name) return -1;

  return name[strlen(name) - 1];
}

const char* fsSwitchGroupGetCanonicalName(uint8_t idx)
{
  static const char fsgroupname[3][4] = {"GR1", "GR2", "GR3"};

  return &fsgroupname[idx][0];
}

SwitchConfig switchGetMaxType(uint8_t idx)
{
  auto hw_type = switchGetHwType(idx);
  if (hw_type == SWITCH_HW_2POS) {
    return SWITCH_2POS;
  } else {
    return SWITCH_3POS;
  }
}

static uint64_t checkSwitchPosition(uint8_t idx, bool startup)
{
  uint64_t result = 0;
  uint32_t index = idx * 3;

  auto pos = switchGetPosition(idx);
  switch(pos) {

  case SWITCH_HW_UP:
    result = ((MASK_CFN_TYPE)1 << index);
    switchesMidposStart[idx] = 0;
    break;

  case SWITCH_HW_DOWN:
    index += 2;
    result = ((MASK_CFN_TYPE)1 << index);
    switchesMidposStart[idx] = 0;
    break;

  case SWITCH_HW_MID:
    if (startup || SWITCH_POSITION(index + 1) ||
        g_eeGeneral.switchesDelay == SWITCHES_DELAY_NONE ||
        (switchesMidposStart[idx] &&
         (tmr10ms_t)(get_tmr10ms() - switchesMidposStart[idx]) >
         SWITCHES_DELAY())) {
      index += 1;
      result = ((MASK_CFN_TYPE)1 << index);
      switchesMidposStart[idx] = 0;
    } else {
      result = (switchesPos & ((MASK_CFN_TYPE)0x7 << index));
      if (!switchesMidposStart[idx]) {
        switchesMidposStart[idx] = get_tmr10ms();
      }
    }
    break;
  }

  if (!(switchesPos & result)) {
    PLAY_SWITCH_MOVED(index);
  }

  return result;
}

void getSwitchesPosition(bool startup)
{
  uint64_t newPos = 0;
  for (unsigned i = 0; i < switchGetMaxAllSwitches(); i++) {
    if (!SWITCH_EXISTS(i)) continue;
    newPos |= checkSwitchPosition(i, startup);
  }

  switchesPos = newPos;

  auto max_pots = adcGetMaxInputs(ADC_INPUT_FLEX);
  auto offset = adcGetInputOffset(ADC_INPUT_FLEX);

  for (int i = 0; i < max_pots; i++) {
    if (IS_POT_MULTIPOS(i)) {
      auto analog_idx = offset + i;
      StepsCalibData * calib = (StepsCalibData *) &g_eeGeneral.calib[analog_idx];
#if defined(SIMU)
      {
        uint8_t count = XPOTS_MULTIPOS_COUNT - 1;
#else
      if (IS_MULTIPOS_CALIBRATED(calib)) {
        uint8_t count = calib->count;
#endif
        uint8_t pos = anaIn(analog_idx) / (2 * RESX / count);
        uint8_t previousPos = potsPos[i] >> 4;
        uint8_t previousStoredPos = potsPos[i] & 0x0F;
        if (startup) {
          potsPos[i] = (pos << 4) | pos;
        }
        else if (pos != previousPos) {
          potsLastposStart[i] = get_tmr10ms();
          potsPos[i] = (pos << 4) | previousStoredPos;
        } else if (g_eeGeneral.switchesDelay == SWITCHES_DELAY_NONE ||
                   (tmr10ms_t)(get_tmr10ms() - potsLastposStart[i]) >
                       SWITCHES_DELAY()) {
          potsLastposStart[i] = 0;
          potsPos[i] = (pos << 4) | pos;
          if (previousStoredPos != pos) {
            PLAY_SWITCH_MOVED(SWSRC_LAST_SWITCH + i * XPOTS_MULTIPOS_COUNT + pos);
          }
        }
      }
    }
  }
}

uint8_t getSwitchCount()
{
  int count = 0;
  for (int i = 0; i < switchGetMaxAllSwitches(); ++i) {
    if (SWITCH_EXISTS(i)) {
      ++count;
    }
  }
  return count;
}

#if !defined(COLORLCD)
uint8_t switchGetMaxRow(uint8_t col)
{
  uint8_t lastrow = 0;
  for (int i = 0; i < switchGetMaxAllSwitches(); ++i) {
    if (SWITCH_EXISTS(i)) {
      auto switch_display = switchGetDisplayPosition(i);
      if (switch_display.col == col)
        lastrow = switch_display.row > lastrow ? switch_display.row : lastrow;
    }
  }
  return lastrow;
}
#endif

bool getSwitch(swsrc_t swtch, uint8_t flags)
{
  bool result;

  if (swtch == SWSRC_NONE)
    return true;

  uint16_t cs_idx = abs(swtch);

  if (cs_idx == SWSRC_ONE) {
    result = !s_mixer_first_run_done;
  }
  else if (cs_idx == SWSRC_ON) {
    result = true;
  }
#if defined(DEBUG_LATENCY)
  else if (cs_idx == SWSRC_LATENCY_TOGGLE) {
    result = latencyToggleSwitch;
  }
#endif
  else if (cs_idx <= SWSRC_LAST_SWITCH) {
    cs_idx -= SWSRC_FIRST_SWITCH;
#if defined(FUNCTION_SWITCHES)
    div_t qr = div(cs_idx, 3);
    if (switchIsCustomSwitch(qr.quot)) {
      auto value = g_model.cfsState(qr.quot);
      result = qr.rem == 0 ? !value : (qr.rem == 2 ? value : false);
    } else
#endif
    {
      div_t qr = div(cs_idx, 3);
      if (SWITCH_EXISTS(qr.quot)) {
        auto sw_cfg = g_model.getSwitchType(qr.quot);
        if (flags & GETSWITCH_MIDPOS_DELAY) {
          result = SWITCH_POSITION(cs_idx);
          // Handle 2POS switch installed in 3POS slot
          if (!result && qr.rem == SWITCH_HW_DOWN && (sw_cfg == SWITCH_2POS || sw_cfg == SWITCH_TOGGLE))
            result = SWITCH_POSITION(cs_idx - 1);
        } else {
          result = switchState(cs_idx);
          // Handle 2POS switch installed in 3POS slot
          if (!result && qr.rem == SWITCH_HW_DOWN && (sw_cfg == SWITCH_2POS || sw_cfg == SWITCH_TOGGLE))
            result = switchState(cs_idx - 1);
        }
      } else {
         result = false;
      }
    }
  }
  else if (cs_idx <= SWSRC_LAST_MULTIPOS_SWITCH) {
    result = POT_POSITION(cs_idx - SWSRC_FIRST_MULTIPOS_SWITCH);
  }
  else if (cs_idx <= SWSRC_LAST_TRIM) {
    uint8_t idx = cs_idx - SWSRC_FIRST_TRIM;
    result = trimDown(idx);
  }
  else if (cs_idx == SWSRC_RADIO_ACTIVITY) {
    result = (inactivity.counter < 2);
  }

  else if (cs_idx >= SWSRC_FIRST_SENSOR && cs_idx <= SWSRC_LAST_SENSOR) {
    result = !telemetryItems[cs_idx-SWSRC_FIRST_SENSOR].isOld();
  }
  else if (cs_idx == SWSRC_TELEMETRY_STREAMING) {
    result = TELEMETRY_STREAMING();
  }

  else {
    return false;
  }

  return swtch > 0 ? result : !result;
}

uint8_t getXPotPosition(uint8_t idx)
{
  if (idx >= MAX_POTS || !IS_POT_MULTIPOS(idx)) return 0;
  return potsPos[idx] & 0x0F;
}

static inline uint8_t _bits_set(uint8_t val, uint8_t bits)
{
  uint8_t bits_set = 0;
  do {
    if (val & 1) ++bits_set;
    val >>= 1;
  } while (--bits);

  return bits_set;
}

swarnstate_t switches_states = 0;

swsrc_t getMovedSwitch()
{
  static tmr10ms_t s_move_last_time = 0;
  swsrc_t result = 0;

  // Switches
  auto max_switches = switchGetMaxAllSwitches();
  for (uint8_t i = 0; i < max_switches; i++) {
    if (SWITCH_EXISTS(i)) {
      swarnstate_t mask = ((swarnstate_t) 0x03 << (i * 2));
      uint8_t prev = (switches_states & mask) >> (i * 2);
      uint8_t next;
#if defined(FUNCTION_SWITCHES)
      if (switchIsCustomSwitch(i))
        next = g_model.cfsState(i) ? 3 : 1;
      else
        next = (1024 + getValue(MIXSRC_FIRST_SWITCH + i)) / 1024 + 1;
#else
      next = (1024 + getValue(MIXSRC_FIRST_SWITCH + i)) / 1024 + 1;
#endif
      if (prev != next) {
        switches_states =
            (switches_states & (~mask)) | ((swarnstate_t)(next) << (i * 2));
        result = (3 * i) + next;
      }
    }
  }

  // Multipos
  for (int i = 0; i < MAX_POTS; i++) {
    if (IS_POT_MULTIPOS(i)) {
      StepsCalibData * calib = (StepsCalibData *) &g_eeGeneral.calib[MAX_STICKS + i];
#if defined(SIMU)
      {
        uint8_t count = XPOTS_MULTIPOS_COUNT - 1;
#else
      if (IS_MULTIPOS_CALIBRATED(calib)) {
        uint8_t count = calib->count;
#endif
        uint8_t prev = potsPos[i] & 0x0F;
        uint8_t next = anaIn(MAX_STICKS + i) / (2 * RESX / count);
        if (prev != next) {
          result = SWSRC_FIRST_MULTIPOS_SWITCH + i * XPOTS_MULTIPOS_COUNT + next;
        }
      }
    }
  }

  // Each physical trim direction is an independent momentary control.
  static uint32_t previousButtons = 0;
  uint32_t buttons = readTrims();
  uint32_t pressed = buttons & ~previousButtons;
  previousButtons = buttons;
  for (int i = 0; i < keysGetMaxTrims() * 2; ++i) {
    if (pressed & (1u << i)) result = SWSRC_FIRST_TRIM + i;
  }

  if ((tmr10ms_t)(get_tmr10ms() - s_move_last_time) > 100)
    result = 0;

  s_move_last_time = get_tmr10ms();
  return result;
}

bool isSwitchWarningRequired(uint16_t &bad_pots)
{
  if (!mixerTaskRunning()) getADC();
  getMovedSwitch();

  bool warn = false;
  for (int i = 0; i < switchGetMaxAllSwitches(); i++) {
    if (SWITCH_WARNING_ALLOWED(i)) {
      uint8_t warnState = g_model.getSwitchWarning(i);
      if (warnState) {
        swarnstate_t swState = g_model.getSwitchStateForWarning(i);
        if (warnState != swState) {
          warn = true;
        }
      }
    }
  }

  if (g_model.potsWarnMode) {
    evalChannelMixes(e_perout_mode_normal, 0);
    bad_pots = 0;
    for (int  i = 0; i < adcGetMaxInputs(ADC_INPUT_FLEX); i++) {
      if (!IS_POT_SLIDER_AVAILABLE(i)) continue;
      if ((g_model.potsWarnEnabled & (1 << i)) &&
          (abs(g_model.potsWarnPosition[i] - GET_LOWRES_POT_POSITION(i)) > 1)) {
        warn = true;
        bad_pots |= (1 << i);
      }
    }
  }

  return warn;
}

#if defined(COLORLCD)
#include "switch_warn_dialog.h"
void checkSwitches()
{
  uint16_t bad_pots = 0;
  if (!isSwitchWarningRequired(bad_pots))
    return;

  LED_ERROR_BEGIN();
  auto dialog = new SwitchWarnDialog();
  MainWindow::instance()->blockUntilClose(true, [=]() {
    return dialog->deleted();
  });
  LED_ERROR_END();
}
#elif defined(GUI)

void checkSwitches()
{
  swarnstate_t last_bad_switches = 0xff;
  uint16_t bad_pots = 0, last_bad_pots = 0xff;

#if defined(PWR_BUTTON_PRESS)
  bool refresh = false;
#endif

  while (true) {
#if defined(RADIO_GX12)
    _poll_switches();
#endif
    if (!isSwitchWarningRequired(bad_pots))
      break;

    cancelSplash();
    LED_ERROR_BEGIN();
    resetBacklightTimeout();

    // first - display warning
    if (last_bad_switches != switches_states || last_bad_pots != bad_pots) {
      drawAlertBox(STR_SWITCHWARN, nullptr, STR_PRESS_ANY_KEY_TO_SKIP);
      if (last_bad_switches == 0xff || last_bad_pots == 0xff) {
        AUDIO_ERROR_MESSAGE(AU_SWITCH_ALERT);
      }
      int x = SWITCH_WARNING_LIST_X;
      int y = SWITCH_WARNING_LIST_Y;
      int numWarnings = 0;
      for (int i = 0; i < switchGetMaxAllSwitches(); ++i) {
        if (SWITCH_WARNING_ALLOWED(i)) {
          uint8_t warnState = g_model.getSwitchWarning(i);
          if (warnState) {
            swarnstate_t swState = g_model.getSwitchStateForWarning(i);
            if (warnState != swState) {
              if (++numWarnings < 6) {
                const char* s = getSwitchWarnSymbol(warnState);
                drawSource(x, y, MIXSRC_FIRST_SWITCH + i, INVERS);
                lcdDrawText(lcdNextPos, y, s, INVERS);
                x = lcdNextPos + 3;
              }
            }
          }
        }
      }

      if (g_model.potsWarnMode) {
        for (int i = 0; i < MAX_POTS; i++) {
          if (!IS_POT_SLIDER_AVAILABLE(i)) continue;
          if (g_model.potsWarnEnabled & (1 << i)) {
            if (abs(g_model.potsWarnPosition[i] - GET_LOWRES_POT_POSITION(i)) > 1) {
              if (++numWarnings < 6) {
                drawSource(x, y, MIXSRC_FIRST_POT + i, INVERS);
                const char* symbol;
                auto warn_pos = g_model.potsWarnPosition[i];
                if (IS_SLIDER(i)) {
                  symbol =  warn_pos > GET_LOWRES_POT_POSITION(i)
                    ? CHAR_UP
                    : CHAR_DOWN;
                } else {
                  symbol =  warn_pos > GET_LOWRES_POT_POSITION(i)
                    ? CHAR_RIGHT
                    : CHAR_LEFT;
                }
                lcdDrawText(lcdNextPos, y, symbol, INVERS);
                x = lcdNextPos + 3;
              }
            }
          }
        }
      }

      if (numWarnings >= 6) {
        lcdDrawText(x, y, "...", 0);
      }

      last_bad_pots = bad_pots;

      lcdRefresh();
      lcdSetContrast();
      waitKeysReleased();

      last_bad_switches = switches_states;
    }

    if (keyDown())
      break;

#if defined(PWR_BUTTON_PRESS)
    uint32_t power = pwrCheck();
    if (power == e_power_off) {
      drawSleepBitmap();
      boardOff();
      break;
    }
    else if (power == e_power_press) {
      refresh = true;
    }
    else if (power == e_power_on && refresh) {
      last_bad_switches = 0xff;
      last_bad_pots = 0xff;
      refresh = false;
    }
#else
    if (pwrCheck() == e_power_off) {
      break;
    }
#endif

    checkBacklight();

    WDG_RESET();
    sleep_ms(10);
  }

  LED_ERROR_END();
}
#endif // GUI

void setAllPreflightSwitchStates()
{
  getMovedSwitch();
  // Mask switches enabled for warnings
  swarnstate_t sw_mask = 0;
  for(uint8_t i = 0; i < switchGetMaxAllSwitches(); i++) {
    if (SWITCH_WARNING_ALLOWED(i))
      if (g_model.getSwitchWarning(i))
        sw_mask |= (0x03 << (2 * i));
  }
  g_model.switchWarning = switches_states & sw_mask;
  AUDIO_WARNING1();
  storageDirty(EE_MODEL);
}
