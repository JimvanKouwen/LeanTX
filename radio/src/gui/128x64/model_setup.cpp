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
#include "mixer_scheduler.h"
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"
#include "hal/module_port.h"
#include "hal/rgbleds.h"
#include "switches.h"

#if defined(USBJ_EX)
#include "usb_joystick.h"
#endif

#include "storage/sdcard_common.h"
#include "storage/modelslist.h"

#if defined(CROSSFIRE)
  #include "telemetry/crossfire.h"
#endif

uint8_t g_moduleIdx;

uint8_t getSwitchWarningsCount()
{
  uint8_t count = 0;
  for (int i = 0; i < switchGetMaxAllSwitches(); ++i) {
    if (SWITCH_WARNING_ALLOWED(i)) {
      ++count;
    }
  }
  return count;
}

enum MenuModelSetupItems {
  ITEM_MODEL_SETUP_NAME,
  ITEM_MODEL_SETUP_TIMER1,
  ITEM_MODEL_SETUP_TIMER1_NAME,
  ITEM_MODEL_SETUP_TIMER1_START,
  ITEM_MODEL_SETUP_TIMER1_PERSISTENT,
  ITEM_MODEL_SETUP_TIMER1_MINUTE_BEEP,
  ITEM_MODEL_SETUP_TIMER1_COUNTDOWN_BEEP,
  ITEM_MODEL_SETUP_TIMER2,
  ITEM_MODEL_SETUP_TIMER2_NAME,
  ITEM_MODEL_SETUP_TIMER2_START,
  ITEM_MODEL_SETUP_TIMER2_PERSISTENT,
  ITEM_MODEL_SETUP_TIMER2_MINUTE_BEEP,
  ITEM_MODEL_SETUP_TIMER2_COUNTDOWN_BEEP,
  ITEM_MODEL_SETUP_TIMER3,
  ITEM_MODEL_SETUP_TIMER3_NAME,
  ITEM_MODEL_SETUP_TIMER3_START,
  ITEM_MODEL_SETUP_TIMER3_PERSISTENT,
  ITEM_MODEL_SETUP_TIMER3_MINUTE_BEEP,
  ITEM_MODEL_SETUP_TIMER3_COUNTDOWN_BEEP,
#if defined(FUNCTION_SWITCHES)
  ITEM_MODEL_SETUP_LABEL,
  ITEM_MODEL_SETUP_SW1,
  ITEM_MODEL_SETUP_SW2,
  ITEM_MODEL_SETUP_SW3,
  ITEM_MODEL_SETUP_SW4,
  ITEM_MODEL_SETUP_SW5,
  ITEM_MODEL_SETUP_SW6,
  ITEM_MODEL_SETUP_SW7,
  ITEM_MODEL_SETUP_SW8,
  ITEM_MODEL_SETUP_SW9,
  ITEM_MODEL_SETUP_SW10,
  ITEM_MODEL_SETUP_SW11,
  ITEM_MODEL_SETUP_SW12,
  ITEM_MODEL_SETUP_SW13,
  ITEM_MODEL_SETUP_SW14,
  ITEM_MODEL_SETUP_SW15,
  ITEM_MODEL_SETUP_SW16,
  ITEM_MODEL_SETUP_SW17,
  ITEM_MODEL_SETUP_SW18,
  ITEM_MODEL_SETUP_SW19,
  ITEM_MODEL_SETUP_SW20,
  ITEM_MODEL_SETUP_GROUP1_LABEL,
  ITEM_MODEL_SETUP_GROUP1_ALWAYS_ON,
  ITEM_MODEL_SETUP_GROUP1_START,
  ITEM_MODEL_SETUP_GROUP2_LABEL,
  ITEM_MODEL_SETUP_GROUP2_ALWAYS_ON,
  ITEM_MODEL_SETUP_GROUP2_START,
  ITEM_MODEL_SETUP_GROUP3_LABEL,
  ITEM_MODEL_SETUP_GROUP3_ALWAYS_ON,
  ITEM_MODEL_SETUP_GROUP3_START,
  ITEM_MODEL_SETUP_GROUP4_LABEL,
  ITEM_MODEL_SETUP_GROUP4_ALWAYS_ON,
  ITEM_MODEL_SETUP_GROUP4_START,
#endif
  ITEM_MODEL_SETUP_EXTENDED_LIMITS,
  ITEM_MODEL_SETUP_EXTENDED_TRIMS,
  ITEM_MODEL_SETUP_DISPLAY_TRIMS,
  ITEM_MODEL_SETUP_TRIM_INC,
  ITEM_MODEL_SETUP_THROTTLE_LABEL,
  ITEM_MODEL_SETUP_THROTTLE_REVERSED,
  ITEM_MODEL_SETUP_THROTTLE_TRACE,
  ITEM_MODEL_SETUP_THROTTLE_TRIM,
  ITEM_MODEL_SETUP_THROTTLE_TRIM_SWITCH,
  ITEM_MODEL_SETUP_PREFLIGHT_LABEL,
  ITEM_MODEL_SETUP_CHECKLIST_DISPLAY,
  ITEM_MODEL_SETUP_CHECKLIST_INTERACTIVE,
  ITEM_MODEL_SETUP_THROTTLE_WARNING,
  ITEM_MODEL_SETUP_CUSTOM_THROTTLE_WARNING,
  ITEM_MODEL_SETUP_CUSTOM_THROTTLE_WARNING_VALUE,
  ITEM_MODEL_SETUP_SWITCHES_WARNING1,
#if defined(PCBTARANIS)
  ITEM_MODEL_SETUP_SWITCHES_WARNING2,
  ITEM_MODEL_SETUP_SWITCHES_WARNING3,
  ITEM_MODEL_SETUP_POTS_WARNING,
#endif
  ITEM_MODEL_SETUP_BEEP_CENTER,
  ITEM_MODEL_SETUP_USE_JITTER_FILTER,

#if defined(HARDWARE_INTERNAL_MODULE)
  ITEM_MODEL_SETUP_INTERNAL_MODULE_LABEL,
#if defined(CROSSFIRE)
  ITEM_MODEL_SETUP_INTERNAL_MODULE_TYPE,
  ITEM_MODEL_SETUP_INTERNAL_MODULE_SERIALSTATUS,
  #if defined(CROSSFIRE)
  ITEM_MODEL_SETUP_INTERNAL_MODULE_ARMING_MODE,
  ITEM_MODEL_SETUP_INTERNAL_MODULE_ARMING_TRIGGER,
  #endif
#endif
  ITEM_MODEL_SETUP_INTERNAL_MODULE_CHANNELS,
#if defined(EXTERNAL_ANTENNA)
  ITEM_MODEL_SETUP_INTERNAL_MODULE_ANTENNA,
#endif
  ITEM_MODEL_SETUP_INTERNAL_MODULE_RECEIVER,
#endif
#if defined(HARDWARE_EXTERNAL_MODULE)
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_LABEL,
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_TYPE,
#if defined(CROSSFIRE)
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_BAUDRATE,
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_SERIALSTATUS,
  #if defined(CROSSFIRE)
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_ARMING_MODE,
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_ARMING_TRIGGER,
  #endif
#endif
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_CHANNELS,
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_RECEIVER,
#endif

  ITEM_VIEW_OPTIONS_LABEL,
  ITEM_VIEW_OPTIONS_RADIO_TAB,
  ITEM_VIEW_OPTIONS_GF,
  ITEM_VIEW_OPTIONS_MODEL_TAB,
  CASE_FLIGHT_MODES(ITEM_VIEW_OPTIONS_FM)
  ITEM_VIEW_OPTIONS_CURVES,
  ITEM_VIEW_OPTIONS_LS,
  ITEM_VIEW_OPTIONS_SF,
#if defined(LUA_MODEL_SCRIPTS)
  ITEM_VIEW_OPTIONS_CUSTOM_SCRIPTS,
#endif
  ITEM_VIEW_OPTIONS_TELEMETRY,

#if defined(USBJ_EX)
  ITEM_MODEL_SETUP_USBJOYSTICK_LABEL,
  ITEM_MODEL_SETUP_USBJOYSTICK_MODE,
  ITEM_MODEL_SETUP_USBJOYSTICK_IF_MODE,
  ITEM_MODEL_SETUP_USBJOYSTICK_CIRC_CUTOUT,
  ITEM_MODEL_SETUP_USBJOYSTICK_CH_BUTTON,
  ITEM_MODEL_SETUP_USBJOYSTICK_APPLY,
#endif

  ITEM_MODEL_SETUP_LINES_COUNT
};

PACK(struct ModelSetupExpandState {
  uint8_t preflight:1;
  uint8_t throttle:1;
  uint8_t viewOpt:1;
  uint8_t functionSwitches:1;
});

static struct ModelSetupExpandState expandState;

static uint8_t PREFLIGHT_ROW(uint8_t value) { return expandState.preflight ? value : HIDDEN_ROW; }

static uint8_t THROTTLE_ROW(uint8_t value) { return expandState.throttle ? value : HIDDEN_ROW; }

#if defined(FUNCTION_SWITCHES)
static uint8_t FS_ROW(uint8_t value) { return expandState.functionSwitches ? value : HIDDEN_ROW; }

uint8_t G1_ROW(int8_t value) { return (firstSwitchInGroup(1) >= 0) ? value : HIDDEN_ROW; }
uint8_t G2_ROW(int8_t value) { return (firstSwitchInGroup(2) >= 0) ? value : HIDDEN_ROW; }
uint8_t G3_ROW(int8_t value) { return (firstSwitchInGroup(3) >= 0) ? value : HIDDEN_ROW; }
uint8_t G4_ROW(int8_t value) { return (firstSwitchInGroup(4) >= 0) ? value : HIDDEN_ROW; }
#endif

static uint8_t VIEWOPT_ROW(uint8_t value) { return expandState.viewOpt ? value : HIDDEN_ROW; }

#define MODEL_SETUP_2ND_COLUMN           (LCD_W-11*FW)
#if defined(CROSSFIRE)
#define IF_MODULE_SYNCED(module, xxx)    (isModuleCrossfire(module) ? (uint8_t)(xxx) : HIDDEN_ROW)
#if SPORT_MAX_BAUDRATE < 400000
#define IF_MODULE_BAUDRATE_ADJUST(module, xxx) (isModuleCrossfire(module) ? (uint8_t)(xxx) : HIDDEN_ROW)
#else
#define IF_MODULE_BAUDRATE_ADJUST(module, xxx) (isModuleCrossfire(module) ? (uint8_t)(xxx) : HIDDEN_ROW)
#endif
#define IF_MODULE_ARMED(module, xxx) (CRSF_ELRS_MIN_VER(module, 4, 0) ? (uint8_t)(xxx) : HIDDEN_ROW)
#define IF_MODULE_ARMED_TRIGGER(module, xxx) ((CRSF_ELRS_MIN_VER(module, 4, 0) && g_model.moduleData[module].crsf.crsfArmingMode) ? (uint8_t)(xxx) : HIDDEN_ROW)
#else
#define IF_MODULE_SYNCED(module, xxx)
#define IF_MODULE_BAUDRATE_ADJUST(module, xxx)
#define IF_MODULE_ARMED(module, xxx)
#define IF_MODULE_ARMED_TRIGGER(module, xxx)
#endif

#if defined(HARDWARE_INTERNAL_MODULE) && defined(HARDWARE_EXTERNAL_MODULE)
  #define CURRENT_MODULE_EDITED(k)        (k >= ITEM_MODEL_SETUP_EXTERNAL_MODULE_LABEL ? EXTERNAL_MODULE : INTERNAL_MODULE)
#elif defined(HARDWARE_INTERNAL_MODULE)
  #define CURRENT_MODULE_EDITED(k)        (INTERNAL_MODULE)
#else
  #define CURRENT_MODULE_EDITED(k)        (EXTERNAL_MODULE)
#endif

#define MAX_SWITCH_PER_LINE             5
  #define SW_WARN_ROWS \
    PREFLIGHT_ROW(uint8_t(NAVIGATION_LINE_BY_LINE|((getSwitchWarningsCount() == 1) ? 1 : getSwitchWarningsCount()-1))), \
    PREFLIGHT_ROW(uint8_t(getSwitchWarningsCount() > MAX_SWITCH_PER_LINE ? TITLE_ROW : HIDDEN_ROW)), \
    PREFLIGHT_ROW(uint8_t(getSwitchWarningsCount() > (MAX_SWITCH_PER_LINE * 2) ? TITLE_ROW : HIDDEN_ROW))

inline uint8_t MODULE_TYPE_ROWS(int moduleIdx)
{
  return 0;
}

inline uint8_t TIMER_ROW(uint8_t timer, uint8_t value)
{
  if (g_model.timers[timer].mode > 0)
    return value;
  return HIDDEN_ROW;
}

#define POT_WARN_ROWS PREFLIGHT_ROW(((g_model.potsWarnMode) ? adcGetMaxInputs(ADC_INPUT_FLEX) : (uint8_t)0))

#define TIMER_ROWS(x)                                                  \
  1, TIMER_ROW(x,0),                                                   \
      TIMER_ROW(x,(uint8_t)((g_model.timers[x].start) ? 2 : 1)),       \
      TIMER_ROW(x,0), TIMER_ROW(x,0),                                  \
      TIMER_ROW(x,g_model.timers[x].countdownBeep != COUNTDOWN_SILENT ? (uint8_t)1 : (uint8_t)0)

#if defined(FUNCTION_SWITCHES)
  #define FUNCTION_SWITCHES_ROWS  0, \
                                  FS_ROW(0), FS_ROW(0), FS_ROW(0), FS_ROW(0), FS_ROW(0),  \
                                  FS_ROW(0), FS_ROW(0), FS_ROW(0), FS_ROW(0), FS_ROW(0),  \
                                  FS_ROW(0), FS_ROW(0), FS_ROW(0), FS_ROW(0), FS_ROW(0),  \
                                  FS_ROW(0), FS_ROW(0), FS_ROW(0), FS_ROW(0), FS_ROW(0),  \
                                  FS_ROW(G1_ROW(LABEL())), \
                                  FS_ROW(G1_ROW(0)),  \
                                  FS_ROW(G1_ROW(0)),  \
                                  FS_ROW(G2_ROW(LABEL())), \
                                  FS_ROW(G2_ROW(0)),  \
                                  FS_ROW(G2_ROW(0)),  \
                                  FS_ROW(G3_ROW(LABEL())), \
                                  FS_ROW(G3_ROW(0)),  \
                                  FS_ROW(G3_ROW(0)),  \
                                  FS_ROW(G4_ROW(LABEL())), \
                                  FS_ROW(G4_ROW(0)),  \
                                  FS_ROW(G4_ROW(0)),
#else
  #define FUNCTION_SWITCHES_ROWS
#endif

#if defined(EXTERNAL_ANTENNA)
#if defined(INTMODULE_ANTSEL_GPIO)
#define EXTERNAL_ANTENNA_ROW  ((g_eeGeneral.antennaMode == ANTENNA_MODE_PER_MODEL) ? (uint8_t)0 : HIDDEN_ROW),
#else
#define EXTERNAL_ANTENNA_ROW  ((isInternalModuleCrossfire() && g_eeGeneral.antennaMode == ANTENNA_MODE_PER_MODEL) ? (uint8_t)0 : HIDDEN_ROW),
#endif
void onModelAntennaSwitchConfirm(const char * result)
{
  if (result == STR_OK) {
    // Switch to external antenna confirmation
    g_model.moduleData[INTERNAL_MODULE].antennaMode = ANTENNA_MODE_EXTERNAL;
    storageDirty(EE_MODEL);
    // Consent already obtained above; mark enabled so checkExternalAntenna()
    // applies the GPIO instead of asking again.
    globalData.externalAntennaEnabled = true;
    checkExternalAntenna();
  }
  else {
    reusableBuffer.moduleSetup.antennaMode = g_model.moduleData[INTERNAL_MODULE].antennaMode;
  }
}
#else
#define EXTERNAL_ANTENNA_ROW
#endif

void editTimerCountdown(int timerIdx, coord_t y, LcdFlags attr, event_t event)
{
  TimerData & timer = g_model.timers[timerIdx];
  lcdDrawTextIndented(y, STR_BEEPCOUNTDOWN);
  int value = timer.countdownBeep;
  if (timer.extraHaptic) value += (COUNTDOWN_VOICE + 1);
  lcdDrawTextAtIndex(MODEL_SETUP_2ND_COLUMN, y, STR_VBEEPCOUNTDOWN, value, (menuHorizontalPosition == 0 ? attr : 0));
  if (timer.countdownBeep != COUNTDOWN_SILENT) {
    lcdDrawNumber(MODEL_SETUP_2ND_COLUMN + 6 * FW, y, TIMER_COUNTDOWN_START(timerIdx), (menuHorizontalPosition == 1 ? attr : 0) | LEFT);
    lcdDrawChar(lcdLastRightPos, y, 's');
  }
  if (attr && s_editMode > 0) {
    switch (menuHorizontalPosition) {
      case 0: 
      {
        value = timer.countdownBeep;
        if (timer.extraHaptic) value += (COUNTDOWN_NON_HAPTIC_LAST + 1);
        CHECK_INCDEC_MODELVAR(event, value, COUNTDOWN_SILENT,
                              COUNTDOWN_COUNT - 1);
        if (value > COUNTDOWN_VOICE + 1) {
          timer.extraHaptic = 1;
          timer.countdownBeep = value - (COUNTDOWN_NON_HAPTIC_LAST + 1);
        } else {
          timer.extraHaptic = 0;
          timer.countdownBeep = value;
        }
      } break;
      case 1:
        timer.countdownStart = -checkIncDecModel(event, -timer.countdownStart, -1, +2);
        break;
    }
  }
}

#include "model_setup_crsf.h"

#if defined(HARDWARE_INTERNAL_MODULE)
#define INTERNAL_MODULE_ROWS \
  LABEL(InternalModule), 0, \
  IF_MODULE_SYNCED(INTERNAL_MODULE, 0), \
  IF_MODULE_ARMED(INTERNAL_MODULE, 0), \
  IF_MODULE_ARMED_TRIGGER(INTERNAL_MODULE, 0), \
  MODULE_CHANNELS_ROWS(INTERNAL_MODULE), \
  EXTERNAL_ANTENNA_ROW \
  MODULE_BIND_ROWS(INTERNAL_MODULE),
#else
#define INTERNAL_MODULE_ROWS
#endif

#if defined(HARDWARE_EXTERNAL_MODULE)
#define EXTERNAL_MODULE_ROWS \
  LABEL(ExternalModule), 0, \
  IF_MODULE_BAUDRATE_ADJUST(EXTERNAL_MODULE, 0), \
  IF_MODULE_SYNCED(EXTERNAL_MODULE, 0), \
  IF_MODULE_ARMED(EXTERNAL_MODULE, 0), \
  IF_MODULE_ARMED_TRIGGER(EXTERNAL_MODULE, 0), \
  MODULE_CHANNELS_ROWS(EXTERNAL_MODULE), \
  MODULE_BIND_ROWS(EXTERNAL_MODULE),
#else
#define EXTERNAL_MODULE_ROWS
#endif

#define WARN_ROWS                         \
  SW_WARN_ROWS,      /* Switch warning */ \
  POT_WARN_ROWS,     /* Pot warning */

#if defined(USBJ_EX)
inline uint8_t USB_JOYSTICK_EXTROW()
{
  return (usbJoystickExtMode() ? (uint8_t)0 : HIDDEN_ROW);
}

inline uint8_t USB_JOYSTICK_APPLYROW()
{
  if(!usbJoystickExtMode()) return HIDDEN_ROW;
  if(usbJoystickSettingsChanged()) return (uint8_t)0;
  return READONLY_ROW;
}

#define USB_JOYSTICK_ROWS                LABEL(USBJoystick), (uint8_t)0, USB_JOYSTICK_EXTROW(), \
                                         USB_JOYSTICK_EXTROW(), USB_JOYSTICK_EXTROW(), USB_JOYSTICK_APPLYROW()
#else
#define USB_JOYSTICK_ROWS
#endif

uint8_t viewOptChoice(coord_t y, const char* title, uint8_t value, uint8_t attr, event_t event)
{
  lcdDrawText(INDENT_WIDTH-1, y, title);
  return editChoice(96, y, nullptr, STR_ADCFILTERVALUES, value, 0, 2, attr, event);
}

#if defined(FUNCTION_SWITCHES)
const char* _fct_sw_start[] = { CHAR_UP, CHAR_DOWN, "=" };
int swIndex;
static uint8_t cfsGroup;

bool checkCFSTypeAvailable(int val)
{
  if (val == SWITCH_3POS) return false;
  int group = g_model.cfsGroup(swIndex);
  if (group > 0 && g_model.cfsGroupAlwaysOn(group) && val == SWITCH_TOGGLE)
    return false;
  return true;
}

static bool checkCFSGroupAvailable(int group)
{
  if (g_model.cfsType(swIndex) == SWITCH_TOGGLE && group && g_model.cfsGroupAlwaysOn(group))
    return false;
  return true;
}

static bool checkCFSSwitchAvailable(int sw)
{
  return (sw == -1) || (sw == switchGetMaxSwitches()) || (switchIsCustomSwitch(sw) && (g_model.cfsGroup(sw) == cfsGroup));
}

enum CFSFields {
  CFS_FIELD_TYPE,
  CFS_FIELD_NAME,
  CFS_FIELD_GROUP,
  CFS_FIELD_START,
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
  CFS_FIELD_COLOR_LABEL,
  CFS_FIELD_ON_COLOR,
  CFS_FIELD_ON_LUA_OVERRIDE,
  CFS_FIELD_OFF_COLOR,
  CFS_FIELD_OFF_LUA_OVERRIDE,
#endif
  CFS_FIELD_COUNT
};

#if defined(FUNCTION_SWITCHES_RGB_LEDS)
bool menuCFSpreview;

static bool checkCFSColorAvailable(int col)
{
  return col > 0;
}

void menuCFSColor(coord_t y, RGBLedColor& color, const char* title, LcdFlags attr, event_t event)
{
  uint8_t selectedColor = getRGBColorIndex(color.getColor());
  selectedColor = editChoice(30, y, title, \
    STR_FS_COLOR_LIST, selectedColor, 0, DIM(colorTable), menuHorizontalPosition == 0 ? attr : 0, event, INDENT_WIDTH, checkCFSColorAvailable);
  if (attr && menuHorizontalPosition == 0 && checkIncDec_Ret) {
    color.setColor(colorTable[selectedColor - 1]);
    storageDirty((isModelMenuDisplayed()) ? EE_MODEL : EE_GENERAL);
  }

  lcdDrawNumber(LCD_W - 6 * FW, y, color.r, (menuHorizontalPosition == 1 ? attr : 0) | RIGHT);
  if (attr && menuHorizontalPosition == 1)
    color.r = checkIncDec(event, color.r, 0, 255, (isModelMenuDisplayed()) ? EE_MODEL : EE_GENERAL);

  lcdDrawNumber(LCD_W - 3 * FW, y, color.g, (menuHorizontalPosition == 2 ? attr : 0) | RIGHT);
  if (attr && menuHorizontalPosition == 2)
    color.g = checkIncDec(event, color.g, 0, 255, (isModelMenuDisplayed()) ? EE_MODEL : EE_GENERAL);

  lcdDrawNumber(LCD_W, y, color.b, (menuHorizontalPosition == 3 ? attr : 0) | RIGHT);
  if (attr && menuHorizontalPosition == 3)
    color.b = checkIncDec(event, color.b, 0, 255, (isModelMenuDisplayed()) ? EE_MODEL : EE_GENERAL);

  if ((attr & BLINK) && !menuCFSpreview) {
    menuCFSpreview = true;
    setFSEditOverride(swIndex, color.getColor());
  }
}
#endif

static void menuModelCFSOne(event_t event)
{
  std::string s(CHAR_SWITCH);
  s += switchGetDefaultName(swIndex);

  int config = g_model.cfsType(swIndex);
  uint8_t group = g_model.cfsGroup(swIndex);
  int startPos = g_model.cfsStart(swIndex);

  SUBMENU(s.c_str(), CFS_FIELD_COUNT,
    {
      0,
      (uint8_t)((config != SWITCH_NONE && config != SWITCH_GLOBAL) ? 0 : HIDDEN_ROW),
      (uint8_t)((config != SWITCH_NONE && config != SWITCH_GLOBAL) ? 0 : HIDDEN_ROW),
      (uint8_t)((config != SWITCH_NONE && config != SWITCH_TOGGLE && config != SWITCH_GLOBAL && group == 0) ? 0 : HIDDEN_ROW),
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
      (uint8_t)((config != SWITCH_NONE && config != SWITCH_GLOBAL) ? LABEL() : HIDDEN_ROW),
      (uint8_t)((config != SWITCH_NONE && config != SWITCH_GLOBAL) ? 3 : HIDDEN_ROW),
      (uint8_t)((config != SWITCH_NONE && config != SWITCH_GLOBAL) ? 0 : HIDDEN_ROW),
      (uint8_t)((config != SWITCH_NONE && config != SWITCH_GLOBAL) ? 3 : HIDDEN_ROW),
      (uint8_t)((config != SWITCH_NONE && config != SWITCH_GLOBAL) ? 0 : HIDDEN_ROW),
#endif
    });

  int8_t sub = menuVerticalPosition;
  int8_t editMode = s_editMode;

  coord_t y = MENU_HEADER_HEIGHT + 1;

#if defined(FUNCTION_SWITCHES_RGB_LEDS)
  menuCFSpreview = false;
#endif

  for (int k = 0; k < NUM_BODY_LINES; k += 1) {
    int i = k + menuVerticalOffset;
    for (int j = 0; j <= i; j += 1) {
      if (j < (int)DIM(mstate_tab) && mstate_tab[j] == HIDDEN_ROW) {
        i += 1;
      }
    }
    LcdFlags attr = (sub == i ? (editMode > 0 ? BLINK | INVERS : INVERS) : 0);

    switch(i) {
      case CFS_FIELD_TYPE:
        config = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_SWITCH_TYPE, STR_SWTYPES, config, SWITCH_NONE, SWITCH_GLOBAL, attr, event, 0, checkCFSTypeAvailable);
        if (attr && checkIncDec_Ret) {
          g_model.cfsSetType(swIndex, (SwitchConfig)config);
          if (config == SWITCH_NONE) {
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
            fsLedRGB(switchGetCustomSwitchIdx(swIndex), 0);
#endif
          } else if (config == SWITCH_TOGGLE) {
            setFSLogicalState(swIndex, 0);
            g_model.cfsSetStart(swIndex, FS_START_PREVIOUS);  // Toggle switches do not have startup position
          }
        }
        break;

      case CFS_FIELD_NAME:
        editSingleName(MODEL_SETUP_2ND_COLUMN, y, STR_NAME, g_model.cfsName(swIndex),
                       LEN_SWITCH_NAME, event, (attr != 0),
                       editMode);
        break;

      case CFS_FIELD_GROUP:
        group = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_SWITCH_GROUP, STR_FSGROUPS, group, 0, NUM_FUNCTIONS_GROUPS, attr, event, 0, checkCFSGroupAvailable);
        if (attr && checkIncDec_Ret) {
          int oldGroup = g_model.cfsGroup(swIndex);
          if (groupHasSwitchOn(group))
            setFSLogicalState(swIndex, 0);
          g_model.cfsSetGroup(swIndex, group);
          if (group > 0) {
            g_model.cfsSetStart(swIndex, groupDefaultSwitch(group) == -1 ? FS_START_PREVIOUS : FS_START_OFF);
            if (config == SWITCH_TOGGLE && g_model.cfsGroupAlwaysOn(group))
              g_model.cfsSetType(swIndex, SWITCH_2POS);
            setGroupSwitchState(group);
          } else {
            g_model.cfsSetStart(swIndex, FS_START_PREVIOUS);
          }
          setGroupSwitchState(oldGroup);
        }
        break;

      case CFS_FIELD_START:
        lcdDrawText(0, y, STR_SWITCH_STARTUP);
        lcdDrawText(MODEL_SETUP_2ND_COLUMN, y, _fct_sw_start[startPos], attr ? (s_editMode ? INVERS + BLINK : INVERS) : 0);
        if (attr) {
          startPos = checkIncDec(event, startPos, FS_START_OFF, FS_START_PREVIOUS, EE_MODEL);
          g_model.cfsSetStart(swIndex, (fsStartPositionType)startPos);
        }
        break;

#if defined(FUNCTION_SWITCHES_RGB_LEDS)
      case CFS_FIELD_COLOR_LABEL:
        lcdDrawText(0, y, STR_BLCOLOR);
        lcdDrawText(LCD_W - 6 * FW, y, "R", RIGHT | SMLSIZE);
        lcdDrawText(LCD_W - 3 * FW, y, "G", RIGHT | SMLSIZE);
        lcdDrawText(LCD_W, y, "B", RIGHT | SMLSIZE);
        break;

      case CFS_FIELD_ON_COLOR:
        menuCFSColor(y, g_model.cfsOnColor(swIndex), STR_OFFON[1], attr, event);
        break;

      case CFS_FIELD_ON_LUA_OVERRIDE:
        g_model.cfsSetOnColorLuaOverride(swIndex, editCheckBox(g_model.cfsOnColorLuaOverride(swIndex), LCD_W - 2 * FW, y, STR_LUA_OVERRIDE, attr, event, INDENT_WIDTH));
        break;

      case CFS_FIELD_OFF_COLOR:
        menuCFSColor(y, g_model.cfsOffColor(swIndex), STR_OFFON[0], attr, event);
        break;

      case CFS_FIELD_OFF_LUA_OVERRIDE:
        g_model.cfsSetOffColorLuaOverride(swIndex, editCheckBox(g_model.cfsOffColorLuaOverride(swIndex), LCD_W - 2 * FW, y, STR_LUA_OVERRIDE, attr, event, INDENT_WIDTH));
        break;
#endif
    }

    y += FH;
  }

#if defined(FUNCTION_SWITCHES_RGB_LEDS)
  if (!menuCFSpreview)
    setFSEditOverride(-1, 0);
#endif
}
#endif

void menuModelSetup(event_t event)
{
  int8_t old_editMode = s_editMode;
  bool CURSOR_ON_CELL = (menuHorizontalPosition >= 0);

#if defined(PCBTARANIS)
  int8_t old_posHorz = menuHorizontalPosition;
#endif

  MENU_TAB({
    HEADER_LINE_COLUMNS
    0,
    TIMER_ROWS(0),
    TIMER_ROWS(1),
    TIMER_ROWS(2),
    FUNCTION_SWITCHES_ROWS
    0, // Extended limits
    1, // Extended trims
    0, // Show trims
    0, // Trims step
    0, // Throttle section
    THROTTLE_ROW(0), // Throttle reverse
    THROTTLE_ROW(0), // Throttle trace source
    THROTTLE_ROW(0), // Throttle trim
    THROTTLE_ROW(0), // Throttle trim switch

    0,   // Preflight section
      PREFLIGHT_ROW(0), // Checklist
      PREFLIGHT_ROW(g_model.displayChecklist ? 0 : HIDDEN_ROW), // Checklist interactive
      PREFLIGHT_ROW(0), // Throttle warning
      PREFLIGHT_ROW(!g_model.disableThrottleWarning ? 0 : HIDDEN_ROW), // Custom position for throttle warning enable
      PREFLIGHT_ROW(!g_model.disableThrottleWarning && g_model.enableCustomThrottleWarning ? 0 : HIDDEN_ROW), // Custom position for throttle warning value
      WARN_ROWS

    uint8_t(NAVIGATION_LINE_BY_LINE | (adcGetInputOffset(ADC_INPUT_FLEX + 1) - 1)), // Center beeps

    0, // ADC Jitter filter

    INTERNAL_MODULE_ROWS

    EXTERNAL_MODULE_ROWS

    // View options
    0,
     VIEWOPT_ROW(LABEL(RadioMenuTabs)),
      VIEWOPT_ROW(0),
      VIEWOPT_ROW(LABEL(ModelMenuTabs)),
      CASE_FLIGHT_MODES(VIEWOPT_ROW(0))
      VIEWOPT_ROW(0),
      VIEWOPT_ROW(0),
      VIEWOPT_ROW(0),
      CASE_LUA_MODEL_SCRIPTS(VIEWOPT_ROW(0))
      VIEWOPT_ROW(0),

    USB_JOYSTICK_ROWS
  });

#if defined(FUNCTION_SWITCHES)
  int swCnt = switchGetMaxSwitches();
  uint8_t* p = (uint8_t*)mstate_tab;
  for (int i = 0; i < MAX_SWITCHES; i += 1) {
    if (i >= swCnt || !switchIsCustomSwitch(i))
      p[i + ITEM_MODEL_SETUP_SW1] = HIDDEN_ROW;
  }
#endif

  MENU_CHECK(menuTabModel, MENU_MODEL_SETUP, HEADER_LINE + ITEM_MODEL_SETUP_LINES_COUNT);
  title(STR_MENU_MODEL_SETUP);

  if (event == EVT_ENTRY || event == EVT_ENTRY_UP) {
    memclear(&reusableBuffer.moduleSetup, sizeof(reusableBuffer.moduleSetup));

#if defined(EXTERNAL_ANTENNA)
    reusableBuffer.moduleSetup.antennaMode = g_model.moduleData[INTERNAL_MODULE].antennaMode;
#endif
  }

  uint8_t sub = menuVerticalPosition - HEADER_LINE;

  for (uint8_t i=0; i<NUM_BODY_LINES; ++i) {
    coord_t y = MENU_HEADER_HEIGHT + 1 + i*FH;
    uint8_t k = i + menuVerticalOffset;
    for (int j=0; j<=k; j++) {
      if (mstate_tab[j+HEADER_LINE] == HIDDEN_ROW) {
        if (++k >= (int)DIM(mstate_tab)) {
          return;
        }
      }
    }

    uint8_t moduleIdx = CURRENT_MODULE_EDITED(k);
    LcdFlags blink = ((s_editMode > 0) ? BLINK|INVERS : INVERS);
    LcdFlags attr = (sub == k ? blink : 0);

    switch (k) {
      case ITEM_MODEL_SETUP_NAME:
        editSingleName(MODEL_SETUP_2ND_COLUMN, y, STR_MODELNAME,
                       g_model.header.name, sizeof(g_model.header.name), event,
                       attr, old_editMode);
        memcpy(modelHeaders[g_eeGeneral.currModel].name, g_model.header.name,
               sizeof(g_model.header.name));
        break;

      case ITEM_MODEL_SETUP_TIMER1:
      case ITEM_MODEL_SETUP_TIMER2:
      case ITEM_MODEL_SETUP_TIMER3:
      {
        unsigned int timerIdx = (k >= ITEM_MODEL_SETUP_TIMER3
                                     ? 2
                                     : (k >= ITEM_MODEL_SETUP_TIMER2 ? 1 : 0));

        TimerData *timer = &g_model.timers[timerIdx];
        drawStringWithIndex(0 * FW, y, STR_TIMER, timerIdx + 1);

        lcdDrawTextAtIndex(MODEL_SETUP_2ND_COLUMN, y, STR_VTMRMODES,
                           timer->mode, menuHorizontalPosition == 0 ? attr : 0);

        drawSwitch(MODEL_SETUP_2ND_COLUMN + 5 * FW, y, timer->swtch,
                   menuHorizontalPosition == 1 ? attr : 0);

        if (attr && s_editMode > 0) {
          switch (menuHorizontalPosition) {
            case 0:
              CHECK_INCDEC_MODELVAR_ZERO(event, timer->mode, TMRMODE_MAX);
              break;
            case 1:
              CHECK_INCDEC_MODELSWITCH(event, timer->swtch, SWSRC_FIRST_IN_MIXES, SWSRC_LAST_IN_MIXES, isSwitchAvailableInMixes);
              break;
          }
        }
        break;
      }

      case ITEM_MODEL_SETUP_TIMER1_START:
      case ITEM_MODEL_SETUP_TIMER2_START:
      case ITEM_MODEL_SETUP_TIMER3_START:
      {
        lcdDrawTextIndented(y, STR_START);

        TimerData *timer =
            &g_model.timers[k >= ITEM_MODEL_SETUP_TIMER3
                                ? 2
                                : (k >= ITEM_MODEL_SETUP_TIMER2 ? 1 : 0)];

        drawTimer(MODEL_SETUP_2ND_COLUMN, y, timer->start,
                  menuHorizontalPosition == 0 ? attr : 0,
                  menuHorizontalPosition == 1 ? attr : 0);

        if (timer->start) {
          lcdDrawTextAtIndex(MODEL_SETUP_2ND_COLUMN + 5 * FW, y, STR_TIMER_DIR,
                             timer->showElapsed,
                             menuHorizontalPosition == 2 ? attr : 0);
        }

        if (attr && s_editMode > 0) {
          div_t qr = div(timer->start, 60);
          switch (menuHorizontalPosition) {
            case 0:
              CHECK_INCDEC_MODELVAR_ZERO(event, qr.quot, 539); // 8:59
              timer->start = qr.rem + qr.quot * 60;
              break;
            case 1:
              qr.rem -= checkIncDecModel(event, qr.rem + 2, 1, 62) - 2;
              timer->start -= qr.rem;
              if ((int16_t) timer->start < 0)
                timer->start = 0;
              if ((int16_t) timer->start > 5999)
                timer->start = 32399; // 8:59:59
              break;
            case 2:
              if (timer->start) {
                timer->showElapsed =
                    checkIncDecModel(event, timer->showElapsed, 0, 1);
              }
              break;
          }
        }
        break;
      }

      case ITEM_MODEL_SETUP_TIMER1_NAME:
      case ITEM_MODEL_SETUP_TIMER2_NAME:
      case ITEM_MODEL_SETUP_TIMER3_NAME:
      {
        TimerData * timer = &g_model.timers[k>=ITEM_MODEL_SETUP_TIMER3 ? 2 : (k>=ITEM_MODEL_SETUP_TIMER2 ? 1 : 0)];
        editSingleName(MODEL_SETUP_2ND_COLUMN, y, STR_NAME, timer->name,
                       sizeof(timer->name), event, attr, old_editMode, INDENT_WIDTH);
        break;
      }

      case ITEM_MODEL_SETUP_TIMER1_MINUTE_BEEP:
      case ITEM_MODEL_SETUP_TIMER2_MINUTE_BEEP:
      case ITEM_MODEL_SETUP_TIMER3_MINUTE_BEEP:
      {
        TimerData * timer = &g_model.timers[k>=ITEM_MODEL_SETUP_TIMER3 ? 2 : (k>=ITEM_MODEL_SETUP_TIMER2 ? 1 : 0)];
        timer->minuteBeep = editCheckBox(timer->minuteBeep, MODEL_SETUP_2ND_COLUMN, y, STR_MINUTEBEEP, attr, event, INDENT_WIDTH);
        break;
      }

      case ITEM_MODEL_SETUP_TIMER1_COUNTDOWN_BEEP:
      case ITEM_MODEL_SETUP_TIMER2_COUNTDOWN_BEEP:
      case ITEM_MODEL_SETUP_TIMER3_COUNTDOWN_BEEP:
      {
        editTimerCountdown(k>=ITEM_MODEL_SETUP_TIMER3 ? 2 : (k>=ITEM_MODEL_SETUP_TIMER2 ? 1 : 0), y, attr, event);
        break;
      }

      case ITEM_MODEL_SETUP_TIMER1_PERSISTENT:
      case ITEM_MODEL_SETUP_TIMER2_PERSISTENT:
      case ITEM_MODEL_SETUP_TIMER3_PERSISTENT:
      {
        TimerData * timer = &g_model.timers[k>=ITEM_MODEL_SETUP_TIMER3 ? 2 : (k>=ITEM_MODEL_SETUP_TIMER2 ? 1 : 0)];
        timer->persistent = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_PERSISTENT, STR_VPERSISTENT, timer->persistent, 0, 2, attr, event, INDENT_WIDTH);
        break;
      }

#if defined(FUNCTION_SWITCHES)
      case ITEM_MODEL_SETUP_LABEL:
        expandState.functionSwitches = expandableSection(y, STR_FUNCTION_SWITCHES, expandState.functionSwitches, attr, event);
        break;

      case ITEM_MODEL_SETUP_SW1:
      case ITEM_MODEL_SETUP_SW2:
      case ITEM_MODEL_SETUP_SW3:
      case ITEM_MODEL_SETUP_SW4:
      case ITEM_MODEL_SETUP_SW5:
      case ITEM_MODEL_SETUP_SW6:
      case ITEM_MODEL_SETUP_SW7:
      case ITEM_MODEL_SETUP_SW8:
      case ITEM_MODEL_SETUP_SW9:
      case ITEM_MODEL_SETUP_SW10:
      case ITEM_MODEL_SETUP_SW11:
      case ITEM_MODEL_SETUP_SW12:
      case ITEM_MODEL_SETUP_SW13:
      case ITEM_MODEL_SETUP_SW14:
      case ITEM_MODEL_SETUP_SW15:
      case ITEM_MODEL_SETUP_SW16:
      case ITEM_MODEL_SETUP_SW17:
      case ITEM_MODEL_SETUP_SW18:
      case ITEM_MODEL_SETUP_SW19:
      case ITEM_MODEL_SETUP_SW20:
      {
        int index = (k - ITEM_MODEL_SETUP_SW1);

        lcdDrawSizedText(INDENT_WIDTH, y, CHAR_SWITCH, 2, attr);
        lcdDrawText(lcdNextPos, y, switchGetDefaultName(index), attr);

        if (attr && event == EVT_KEY_BREAK(KEY_ENTER)) {
          swIndex = index;
          pushMenu(menuModelCFSOne);
        }

        int config = g_model.cfsType(index);
        lcdDrawText(30 + 5 * FW, y, STR_SWTYPES[config]);

        if (config != SWITCH_NONE && config != SWITCH_GLOBAL) {
          if (g_model.cfsName(index)[0]) {
          char s[LEN_SWITCH_NAME + 1];
          strAppend(s, g_model.cfsName(index), LEN_SWITCH_NAME);
          lcdDrawText(35, y, s);
          } else {
            lcdDrawMMM(35, y, 0);
          }

          uint8_t group = g_model.cfsGroup(index);
          lcdDrawText(30 + 13 * FW, y, STR_FSGROUPS[group]);

          if (config != SWITCH_TOGGLE && group == 0) {
            int startPos = g_model.cfsStart(index);
            lcdDrawText(30 + 15 * FW, y, _fct_sw_start[startPos]);
          }
        }
        break;
      }

      case ITEM_MODEL_SETUP_GROUP1_LABEL:
      case ITEM_MODEL_SETUP_GROUP2_LABEL:
      case ITEM_MODEL_SETUP_GROUP3_LABEL:
      case ITEM_MODEL_SETUP_GROUP4_LABEL:
        {
          int group = (k - ITEM_MODEL_SETUP_GROUP1_LABEL) / 3 + 1;
          lcdDrawText(INDENT_WIDTH, y, STR_GROUP);
          lcdDrawNumber(lcdNextPos, y, group, 0);
        }
        break;

      case ITEM_MODEL_SETUP_GROUP1_ALWAYS_ON:
      case ITEM_MODEL_SETUP_GROUP2_ALWAYS_ON:
      case ITEM_MODEL_SETUP_GROUP3_ALWAYS_ON:
      case ITEM_MODEL_SETUP_GROUP4_ALWAYS_ON:
        {
          uint8_t group = (k - ITEM_MODEL_SETUP_GROUP1_ALWAYS_ON) / 3 + 1;
          lcdDrawText(INDENT_WIDTH * 2, y, STR_GROUP_ALWAYS_ON);
          int groupAlwaysOn = g_model.cfsGroupAlwaysOn(group);
          groupAlwaysOn = editCheckBox(groupAlwaysOn, MODEL_SETUP_2ND_COLUMN, y, nullptr, attr, event);
          if (attr && checkIncDec_Ret) {
            g_model.cfsSetGroupAlwaysOn(group, groupAlwaysOn);
            setGroupSwitchState(group);
          }
        }
        break;

      case ITEM_MODEL_SETUP_GROUP1_START:
      case ITEM_MODEL_SETUP_GROUP2_START:
      case ITEM_MODEL_SETUP_GROUP3_START:
      case ITEM_MODEL_SETUP_GROUP4_START:
        {
          uint8_t group = (k - ITEM_MODEL_SETUP_GROUP1_START) / 3 + 1;
          lcdDrawText(INDENT_WIDTH * 2, y, STR_START);
          int sw = groupDefaultSwitch(group);
          cfsGroup = group;
          if (sw == -1)
            lcdDrawText(MODEL_SETUP_2ND_COLUMN + 1, y, "=", attr);
          else if (sw == switchGetMaxSwitches())
            lcdDrawText(MODEL_SETUP_2ND_COLUMN + 1, y, STR_OFF, attr);
          else
            lcdDrawText(MODEL_SETUP_2ND_COLUMN + 1, y, switchGetDefaultName(sw), attr);
          sw = checkIncDec(event, sw, -1, switchGetMaxSwitches() - (g_model.cfsGroupAlwaysOn(group) ? 1 : 0), EE_MODEL, checkCFSSwitchAvailable);
          if (attr && checkIncDec_Ret) {
            for (int i = 0; i < switchGetMaxSwitches(); i += 1) {
              if (switchIsCustomSwitch(i) && g_model.cfsGroup(i) == group) {
                g_model.cfsSetStart(i, (sw >= 0) ? FS_START_OFF : FS_START_PREVIOUS);
              }
            }
            if (sw >= 0 && sw < switchGetMaxSwitches()) {
              g_model.cfsSetStart(sw, FS_START_ON);
            }
          }
        }
        break;
#endif

      case ITEM_MODEL_SETUP_EXTENDED_LIMITS:
        g_model.extendedLimits = editCheckBox(g_model.extendedLimits, MODEL_SETUP_2ND_COLUMN, y, STR_ELIMITS, attr, event);
        break;

      case ITEM_MODEL_SETUP_EXTENDED_TRIMS:
        g_model.extendedTrims = editCheckBox(g_model.extendedTrims, MODEL_SETUP_2ND_COLUMN, y, STR_ETRIMS, menuHorizontalPosition<=0 ? attr : 0, event==EVT_KEY_BREAK(KEY_ENTER) ? event : 0);
        lcdDrawText(MODEL_SETUP_2ND_COLUMN+4*FW, y, STR_RESET_BTN, (menuHorizontalPosition>0  && !NO_HIGHLIGHT()) ? attr : 0);
        if (attr && menuHorizontalPosition>0) {
          s_editMode = 0;
          if (event==EVT_KEY_LONG(KEY_ENTER)) {
            killEvents(event);
            START_NO_HIGHLIGHT();
            for (uint8_t i=0; i<MAX_FLIGHT_MODES; i++) {
              memclear(&g_model.flightModeData[i], TRIMS_ARRAY_SIZE);
            }
            storageDirty(EE_MODEL);
            AUDIO_WARNING1();
          }
        }
        break;

      case ITEM_MODEL_SETUP_DISPLAY_TRIMS:
        g_model.displayTrims = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_DISPLAY_TRIMS, STR_VDISPLAYTRIMS, g_model.displayTrims, 0, 2, attr, event);
        break;

      case ITEM_MODEL_SETUP_TRIM_INC:
        g_model.trimInc = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_TRIMINC, STR_VTRIMINC, g_model.trimInc, -2, 2, attr, event);
        break;

      case ITEM_MODEL_SETUP_THROTTLE_LABEL:
        expandState.throttle = expandableSection(y, STR_THROTTLE_LABEL, expandState.throttle, attr, event);
        break;

      case ITEM_MODEL_SETUP_THROTTLE_REVERSED:
        g_model.throttleReversed = editCheckBox(g_model.throttleReversed, MODEL_SETUP_2ND_COLUMN+20, y, STR_THROTTLEREVERSE, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_THROTTLE_TRACE:
      {
        lcdDrawTextIndented(y, STR_TTRACE);
        if (attr)
          CHECK_INCDEC_MODELVAR_ZERO_CHECK(
              event, g_model.thrTraceSrc,
              adcGetMaxInputs(ADC_INPUT_FLEX) + MAX_OUTPUT_CHANNELS,
              isThrottleSourceAvailable);

        uint8_t idx = throttleSource2Source(g_model.thrTraceSrc);
        drawSource(MODEL_SETUP_2ND_COLUMN+20, y, idx, attr);
        break;
      }

      case ITEM_MODEL_SETUP_THROTTLE_TRIM:
        g_model.thrTrim = editCheckBox(g_model.thrTrim, MODEL_SETUP_2ND_COLUMN+20, y, STR_TTRIM, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_THROTTLE_TRIM_SWITCH:
        lcdDrawTextIndented(y, STR_TTRIM_SW);
        if (attr)
          CHECK_INCDEC_MODELVAR_ZERO(event, g_model.thrTrimSw, keysGetMaxTrims() - 1);
        drawSource(MODEL_SETUP_2ND_COLUMN+20, y, g_model.getThrottleStickTrimSource(), attr);
        break;

      case ITEM_MODEL_SETUP_PREFLIGHT_LABEL:
        expandState.preflight = expandableSection(y, STR_PREFLIGHT, expandState.preflight, attr, event);
        break;

      case ITEM_MODEL_SETUP_CHECKLIST_DISPLAY:
        g_model.displayChecklist = editCheckBox(g_model.displayChecklist, MODEL_SETUP_2ND_COLUMN, y, STR_CHECKLIST, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_CHECKLIST_INTERACTIVE:
        g_model.checklistInteractive = editCheckBox(g_model.checklistInteractive, MODEL_SETUP_2ND_COLUMN, y, STR_CHECKLIST_INTERACTIVE, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_THROTTLE_WARNING:
        g_model.disableThrottleWarning = !editCheckBox(!g_model.disableThrottleWarning, MODEL_SETUP_2ND_COLUMN, y, STR_THROTTLE_WARNING, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_CUSTOM_THROTTLE_WARNING:
        g_model.enableCustomThrottleWarning = editCheckBox(g_model.enableCustomThrottleWarning, MODEL_SETUP_2ND_COLUMN, y, STR_CUSTOM_THROTTLE_WARNING, attr, event, INDENT_WIDTH*4);
        break;

      case ITEM_MODEL_SETUP_CUSTOM_THROTTLE_WARNING_VALUE:
        g_model.customThrottleWarningPosition = editNumberField(STR_CUSTOM_THROTTLE_WARNING_VAL, INDENT_WIDTH * 4, MODEL_SETUP_2ND_COLUMN, y,
                                                  g_model.customThrottleWarningPosition, -100, 100, attr, event);
        break;

      case ITEM_MODEL_SETUP_SWITCHES_WARNING2:
      case ITEM_MODEL_SETUP_SWITCHES_WARNING3:
        if (i==0) {
          if (IS_PREVIOUS_EVENT(event))
            menuVerticalOffset--;
          else
            menuVerticalOffset++;
        }
        break;

      case ITEM_MODEL_SETUP_SWITCHES_WARNING1:
        {
          uint8_t switchWarningsCount = getSwitchWarningsCount();
          if (attr && switchWarningsCount == 1 && menuHorizontalPosition >= 1)
            menuHorizontalPosition = 0;
          horzpos_t l_posHorz = menuHorizontalPosition;

          if (i>=NUM_BODY_LINES-2 && getSwitchWarningsCount() > MAX_SWITCH_PER_LINE*(NUM_BODY_LINES-i)) {
            if (IS_PREVIOUS_EVENT(event))
              menuVerticalOffset--;
            else
              menuVerticalOffset++;
            break;
          }

          lcdDrawTextIndented(y, STR_SWITCHWARNING);
          if (attr) {
            s_editMode = 0;
            switch (event) {
              case EVT_KEY_LONG(KEY_ENTER):
                killEvents(event);
                if (menuHorizontalPosition < 0 ||
                    menuHorizontalPosition >= switchWarningsCount) {
                  START_NO_HIGHLIGHT();
                  setAllPreflightSwitchStates();
                }
                break;
            }
          }

          int current = 0;
          for (int i = 0; i < switchGetMaxAllSwitches(); i++) {
            if (SWITCH_WARNING_ALLOWED(i)) {
              div_t qr = div(current, MAX_SWITCH_PER_LINE);
              if (event == EVT_KEY_BREAK(KEY_ENTER) && attr &&
                  l_posHorz == current && old_posHorz >= 0) {
                uint8_t curr_state = g_model.getSwitchWarning(i);
                // add the new one (if switch UP and 2POS, jump directly to DOWN)
                curr_state += (curr_state != 1 || IS_CONFIG_3POS(i) ? 1 : 2);
                g_model.setSwitchWarning(i, curr_state);
                storageDirty(EE_MODEL);
              }

              lcdDrawChar(
                  MODEL_SETUP_2ND_COLUMN + qr.rem * ((2 * FW) + 1),
                  y + FH * qr.quot, switchGetLetter(i),
                  attr && (menuHorizontalPosition == current) ? INVERS : 0);
              lcdDrawText(lcdNextPos, y + FH * qr.quot,
                          getSwitchWarnSymbol(g_model.getSwitchWarning(i)));
              ++current;
            }
          }
          if (attr && ((menuHorizontalPosition < 0) ||
                       menuHorizontalPosition >= switchWarningsCount)) {
            lcdDrawFilledRect(MODEL_SETUP_2ND_COLUMN - 1, y - 1,
                              8 * (2 * FW + 1), 1 + FH * ((current + 4) / 5));
          }
        break;
      }

      case ITEM_MODEL_SETUP_POTS_WARNING:
        lcdDrawTextIndented(y, STR_POTWARNING);
        lcdDrawTextAtIndex(MODEL_SETUP_2ND_COLUMN, y, STR_PREFLIGHT_POTSLIDER_CHECK, g_model.potsWarnMode, (menuHorizontalPosition == 0) ? attr : 0);
        if (attr && (menuHorizontalPosition == 0)) {
          CHECK_INCDEC_MODELVAR(event, g_model.potsWarnMode, POTS_WARN_OFF, POTS_WARN_AUTO);
          storageDirty(EE_MODEL);
        }

        if (attr) {
          if (menuHorizontalPosition > 0) s_editMode = 0;
          if (menuHorizontalPosition > 0) {
            switch (event) {
              case EVT_KEY_LONG(KEY_ENTER):
                killEvents(event);
                if (g_model.potsWarnMode == POTS_WARN_MANUAL) {
                  SAVE_POT_POSITION(menuHorizontalPosition-1);
                  AUDIO_WARNING1();
                  storageDirty(EE_MODEL);
                }
                break;
              case EVT_KEY_BREAK(KEY_ENTER):
                g_model.potsWarnEnabled ^= (1 << (menuHorizontalPosition-1));
                storageDirty(EE_MODEL);
                break;
            }
          }
        }
        if (g_model.potsWarnMode) {
          coord_t x = MODEL_SETUP_2ND_COLUMN+28;
          uint8_t max_pots = adcGetMaxInputs(ADC_INPUT_FLEX);
          for (int i = 0; i < max_pots; ++i) {

            if (!IS_POT_SLIDER_AVAILABLE(i)) {
              // skip non configured pot
              if (attr && (menuHorizontalPosition==i+1)) repeatLastCursorMove(event);
            }
            else {
              LcdFlags flags = ((menuHorizontalPosition==i+1) && attr) ? BLINK : 0;
              if ((!attr || menuHorizontalPosition >= 0) &&
                  (g_model.potsWarnEnabled & (1 << i))) {
                flags |= INVERS;
              }
              if (max_pots > 3) {
                lcdDrawText(x, y, getAnalogShortLabel(adcGetInputOffset(ADC_INPUT_FLEX) + i), flags);
                x = lcdNextPos + 1;
              }
              else {
                lcdDrawText(x, y, getPotLabel(i), flags);
                x = lcdNextPos + 3;
              }
            }
          }
        }
        break;

      case ITEM_MODEL_SETUP_BEEP_CENTER: {
        lcdDrawTextAlignedLeft(y, STR_BEEPCTR);
        uint8_t pot_offset = adcGetInputOffset(ADC_INPUT_FLEX);
        uint8_t input_max = adcGetMaxInputs(ADC_INPUT_MAIN) + adcGetMaxInputs(ADC_INPUT_FLEX);
        coord_t x = MODEL_SETUP_2ND_COLUMN;
        for (uint8_t i = 0; i < input_max; i++) {
          if ( i >= pot_offset && (IS_POT_MULTIPOS(i - pot_offset) || !IS_POT_SLIDER_AVAILABLE(i - pot_offset)) ) {
            if (attr && menuHorizontalPosition == i) repeatLastCursorMove(event);
            continue;
          }
          LcdFlags flags = 0;
          if ((menuHorizontalPosition == i) && attr)
            flags = BLINK | INVERS;
          else if (ANALOG_CENTER_BEEP(i) || (attr && CURSOR_ON_LINE()))
            flags = INVERS;
          lcdDrawText(x, y, getAnalogShortLabel(i), flags);
          x = lcdNextPos;
        }
        if (attr && CURSOR_ON_CELL) {
          if (event == EVT_KEY_BREAK(KEY_ENTER)) {
            s_editMode = 0;
            g_model.beepANACenter ^= ((BeepANACenter)1<<menuHorizontalPosition);
            storageDirty(EE_MODEL);
          }
        }
      } break;

      case ITEM_MODEL_SETUP_USE_JITTER_FILTER:
        g_model.jitterFilter = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_JITTER_FILTER, STR_ADCFILTERVALUES, g_model.jitterFilter, 0, 2, attr, event);
        break;

#if defined(HARDWARE_INTERNAL_MODULE)
      case ITEM_MODEL_SETUP_INTERNAL_MODULE_LABEL:
        lcdDrawTextAlignedLeft(y, STR_INTERNALRF);
        break;
#endif

#if defined(HARDWARE_EXTERNAL_MODULE)
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_LABEL:
        lcdDrawTextAlignedLeft(y, STR_EXTERNALRF);
        break;
#endif

#if defined(HARDWARE_INTERNAL_MODULE)
      case ITEM_MODEL_SETUP_INTERNAL_MODULE_TYPE:
#endif
#if defined(HARDWARE_EXTERNAL_MODULE)
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_TYPE:
#endif
        editCrsfModuleType(moduleIdx, y, attr, event);
        break;

#if (defined(HARDWARE_EXTERNAL_MODULE)) && (defined(CROSSFIRE))
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_BAUDRATE: {
        ModuleData & moduleData = g_model.moduleData[moduleIdx];
        lcdDrawTextIndented(y, STR_BAUDRATE);
        if (isModuleCrossfire(EXTERNAL_MODULE)) {
          lcdDrawTextAtIndex(MODEL_SETUP_2ND_COLUMN, y, STR_CRSF_BAUDRATE, CROSSFIRE_STORE_TO_INDEX(moduleData.crsf.telemetryBaudrate),attr | LEFT);
          if (attr) {
            moduleData.crsf.telemetryBaudrate =CROSSFIRE_INDEX_TO_STORE(checkIncDecModel(event,CROSSFIRE_STORE_TO_INDEX(moduleData.crsf.telemetryBaudrate),0, DIM(CROSSFIRE_BAUDRATES) - 1));
            if (checkIncDec_Ret) {
              restartModule(moduleIdx);
            }
          }
        }

        break;
      }
#endif
#if defined(CROSSFIRE)
#if defined(HARDWARE_INTERNAL_MODULE)
      case ITEM_MODEL_SETUP_INTERNAL_MODULE_SERIALSTATUS:
#endif
#if defined(HARDWARE_EXTERNAL_MODULE)
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_SERIALSTATUS:
#endif
        lcdDrawTextIndented(y, STR_STATUS);
        lcdDrawNumber(MODEL_SETUP_2ND_COLUMN, y, 1000000 / getMixerSchedulerPeriod(), LEFT | attr);
        lcdDrawText(lcdNextPos, y, "Hz ", attr);
        break;
#endif

#if defined(CROSSFIRE)
#if defined(HARDWARE_INTERNAL_MODULE)
      case ITEM_MODEL_SETUP_INTERNAL_MODULE_ARMING_MODE:
#endif
#if defined(HARDWARE_EXTERNAL_MODULE)
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_ARMING_MODE:
#endif 
        g_model.moduleData[moduleIdx].crsf.crsfArmingMode = 
          editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_CRSF_ARMING_MODE, STR_CRSF_ARMING_MODES, 
          g_model.moduleData[moduleIdx].crsf.crsfArmingMode, ARMING_MODE_FIRST, ARMING_MODE_LAST, attr, event, INDENT_WIDTH);
        break;

#if defined(HARDWARE_INTERNAL_MODULE)
      case ITEM_MODEL_SETUP_INTERNAL_MODULE_ARMING_TRIGGER:
#endif
#if defined(HARDWARE_EXTERNAL_MODULE)
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_ARMING_TRIGGER:
#endif
        lcdDrawTextIndented(y, STR_SWITCH);
        drawSwitch(MODEL_SETUP_2ND_COLUMN, y, g_model.moduleData[moduleIdx].crsf.crsfArmingTrigger, attr);
        if(attr)
          CHECK_INCDEC_SWITCH(event, g_model.moduleData[moduleIdx].crsf.crsfArmingTrigger, SWSRC_FIRST, SWSRC_LAST, EE_MODEL, isSwitchAvailableForArming);
        break;
#endif

#if defined(HARDWARE_INTERNAL_MODULE)
      case ITEM_MODEL_SETUP_INTERNAL_MODULE_CHANNELS:
#endif
#if defined(HARDWARE_EXTERNAL_MODULE)
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_CHANNELS:
#endif
            {
        uint8_t moduleIdx = CURRENT_MODULE_EDITED(k);
        editCrsfChannels(moduleIdx, y, attr, event);
        break;
      }

#if defined(HARDWARE_INTERNAL_MODULE)
      case ITEM_MODEL_SETUP_INTERNAL_MODULE_RECEIVER:
#endif
#if defined(HARDWARE_EXTERNAL_MODULE)
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_RECEIVER:
#endif
            {
        uint8_t moduleIdx = CURRENT_MODULE_EDITED(k);
        editCrsfReceiver(moduleIdx, y, attr, event);
        break;
      }

#if defined(EXTERNAL_ANTENNA)
      case ITEM_MODEL_SETUP_INTERNAL_MODULE_ANTENNA:
        reusableBuffer.moduleSetup.antennaMode = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_ANTENNA, STR_ANTENNA_MODES,
                                                            reusableBuffer.moduleSetup.antennaMode == ANTENNA_MODE_PER_MODEL ? ANTENNA_MODE_INTERNAL : reusableBuffer.moduleSetup.antennaMode,
                                                            ANTENNA_MODE_INTERNAL, ANTENNA_MODE_EXTERNAL, attr, event, INDENT_WIDTH,
                                                            [](int value) { return value != ANTENNA_MODE_PER_MODEL; });
        if (event && !s_editMode && reusableBuffer.moduleSetup.antennaMode != g_model.moduleData[INTERNAL_MODULE].antennaMode) {
          if (reusableBuffer.moduleSetup.antennaMode == ANTENNA_MODE_EXTERNAL && !isExternalAntennaEnabled()) {
            POPUP_CONFIRMATION(STR_ANTENNACONFIRM1, onModelAntennaSwitchConfirm);
            SET_WARNING_INFO(STR_ANTENNACONFIRM2, strlen(STR_ANTENNACONFIRM2), 0);
          }
          else {
            g_model.moduleData[INTERNAL_MODULE].antennaMode = reusableBuffer.moduleSetup.antennaMode;
            checkExternalAntenna();
          }
        }
        break;
#endif

      case ITEM_VIEW_OPTIONS_LABEL:
        expandState.viewOpt = expandableSection(y, STR_ENABLED_FEATURES, expandState.viewOpt, attr, event);
        break;
      case ITEM_VIEW_OPTIONS_RADIO_TAB:
        lcdDrawText(INDENT_WIDTH-2, y, STR_RADIO_MENU_TABS);
        break;
      case ITEM_VIEW_OPTIONS_GF:
        g_model.radioGFDisabled = viewOptChoice(y, STR_MENUSPECIALFUNCS, g_model.radioGFDisabled, attr, event);
        break;

      case ITEM_VIEW_OPTIONS_MODEL_TAB:
        lcdDrawText(INDENT_WIDTH-2, y, STR_MODEL_MENU_TABS);
        break;
#if defined(FLIGHT_MODES)
      case ITEM_VIEW_OPTIONS_FM:
        g_model.modelFMDisabled = viewOptChoice(y, STR_MENUFLIGHTMODES, g_model.modelFMDisabled, attr, event);
        break;
#endif
      case ITEM_VIEW_OPTIONS_CURVES:
        g_model.modelCurvesDisabled = viewOptChoice(y, STR_MENUCURVES, g_model.modelCurvesDisabled, attr, event);
        break;
      case ITEM_VIEW_OPTIONS_LS:
        g_model.modelLSDisabled = viewOptChoice(y, STR_MENULOGICALSWITCHES, g_model.modelLSDisabled, attr, event);
        break;
      case ITEM_VIEW_OPTIONS_SF:
        g_model.modelSFDisabled = viewOptChoice(y, STR_MENUCUSTOMFUNC, g_model.modelSFDisabled, attr, event);
        break;
#if defined(LUA_MODEL_SCRIPTS)
      case ITEM_VIEW_OPTIONS_CUSTOM_SCRIPTS:
        g_model.modelCustomScriptsDisabled = viewOptChoice(y, STR_MENUCUSTOMSCRIPTS, g_model.modelCustomScriptsDisabled, attr, event);
        break;
#endif
      case ITEM_VIEW_OPTIONS_TELEMETRY:
        g_model.modelTelemetryDisabled = viewOptChoice(y, STR_MENUTELEMETRY, g_model.modelTelemetryDisabled, attr, event);
        break;

#if defined(USBJ_EX)
      case ITEM_MODEL_SETUP_USBJOYSTICK_LABEL:
        lcdDrawTextAlignedLeft(y, STR_USBJOYSTICK_LABEL);
        break;

      case ITEM_MODEL_SETUP_USBJOYSTICK_MODE:
        g_model.usbJoystickExtMode = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_USBJOYSTICK_EXTMODE, STR_VUSBJOYSTICK_EXTMODE, g_model.usbJoystickExtMode, 0, 1, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_USBJOYSTICK_IF_MODE:
        g_model.usbJoystickIfMode = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_USBJOYSTICK_IF_MODE, STR_VUSBJOYSTICK_IF_MODE, g_model.usbJoystickIfMode, 0, USBJOYS_LAST, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_USBJOYSTICK_CIRC_CUTOUT:
        g_model.usbJoystickCircularCut = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_USBJOYSTICK_CIRC_COUTOUT, STR_VUSBJOYSTICK_CIRC_COUTOUT, g_model.usbJoystickCircularCut, 0, USBJOYS_CC_LAST, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_USBJOYSTICK_CH_BUTTON:
        lcdDrawText(INDENT_WIDTH, y, STR_USBJOYSTICK_SETTINGS, attr);
        if (attr && event == EVT_KEY_BREAK(KEY_ENTER)) {
          pushMenu(menuModelUSBJoystick);
        }
        break;

      case ITEM_MODEL_SETUP_USBJOYSTICK_APPLY:
        lcdDrawText(INDENT_WIDTH, y, STR_USBJOYSTICK_APPLY_CHANGES, attr);
        if (attr && event == EVT_KEY_BREAK(KEY_ENTER)) {
          onUSBJoystickModelChanged();
        }
        break;
#endif
    }
  }

  // some field just finished being edited
  if (old_editMode > 0 && s_editMode == 0) {
    switch(menuVerticalPosition) {
#if defined(HARDWARE_INTERNAL_MODULE)
      case ITEM_MODEL_SETUP_INTERNAL_MODULE_RECEIVER:
        if (menuHorizontalPosition == 0)
          checkModelIdUnique(g_eeGeneral.currModel, INTERNAL_MODULE);
        break;
#endif
#if defined(HARDWARE_EXTERNAL_MODULE)
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_RECEIVER:
        if (menuHorizontalPosition == 0)
          checkModelIdUnique(g_eeGeneral.currModel, EXTERNAL_MODULE);
        break;
#endif
    }
  }
}
