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

#include "hal/adc_driver.h"
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"
#include "hal/module_port.h"

#include "edgetx.h"
#include "mixer_scheduler.h"
#include "switches.h"

#if defined(USBJ_EX)
#include "usb_joystick.h"
#endif

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
  ITEM_MODEL_SETUP_BITMAP,
  ITEM_MODEL_SETUP_TIMER1,
  ITEM_MODEL_SETUP_TIMER1_NAME,
  ITEM_MODEL_SETUP_TIMER1_START,
  ITEM_MODEL_SETUP_TIMER1_PERSISTENT,
  ITEM_MODEL_SETUP_TIMER1_MINUTE_BEEP,
  ITEM_MODEL_SETUP_TIMER1_COUNTDOWN_BEEP,
#if TIMERS > 1
  ITEM_MODEL_SETUP_TIMER2,
  ITEM_MODEL_SETUP_TIMER2_NAME,
  ITEM_MODEL_SETUP_TIMER2_START,
  ITEM_MODEL_SETUP_TIMER2_PERSISTENT,
  ITEM_MODEL_SETUP_TIMER2_MINUTE_BEEP,
  ITEM_MODEL_SETUP_TIMER2_COUNTDOWN_BEEP,
#endif
#if TIMERS > 2
  ITEM_MODEL_SETUP_TIMER3,
  ITEM_MODEL_SETUP_TIMER3_NAME,
  ITEM_MODEL_SETUP_TIMER3_START,
  ITEM_MODEL_SETUP_TIMER3_PERSISTENT,
  ITEM_MODEL_SETUP_TIMER3_MINUTE_BEEP,
  ITEM_MODEL_SETUP_TIMER3_COUNTDOWN_BEEP,
#endif
#if defined(PCBX9E)
  ITEM_MODEL_SETUP_TOP_LCD_TIMER,
#endif
  ITEM_MODEL_SETUP_EXTENDED_LIMITS,
  ITEM_MODEL_SETUP_THROTTLE_LABEL,
  ITEM_MODEL_SETUP_THROTTLE_REVERSED,
  ITEM_MODEL_SETUP_THROTTLE_TRACE,
  ITEM_MODEL_SETUP_PREFLIGHT_LABEL,
  ITEM_MODEL_SETUP_CHECKLIST_DISPLAY,
  ITEM_MODEL_SETUP_CHECKLIST_INTERACTIVE,
  ITEM_MODEL_SETUP_THROTTLE_WARNING,
  ITEM_MODEL_SETUP_CUSTOM_THROTTLE_WARNING,
  ITEM_MODEL_SETUP_CUSTOM_THROTTLE_WARNING_VALUE,
  ITEM_MODEL_SETUP_SWITCHES_WARNING1,
#if defined(PCBX9E)
  ITEM_MODEL_SETUP_SWITCHES_WARNING2,
  ITEM_MODEL_SETUP_SWITCHES_WARNING3,
#endif
  ITEM_MODEL_SETUP_POTS_WARNING,
#if defined(PCBX9E)
  ITEM_MODEL_SETUP_POTS_WARNING2,
#endif
  ITEM_MODEL_SETUP_BEEP_CENTER,
  ITEM_MODEL_SETUP_USE_JITTER_FILTER,
  ITEM_MODEL_SETUP_INTERNAL_MODULE_LABEL,
  ITEM_MODEL_SETUP_INTERNAL_MODULE_TYPE,
  ITEM_MODEL_SETUP_INTERNAL_MODULE_CHANNELS,
  ITEM_MODEL_SETUP_INTERNAL_MODULE_RECEIVER,
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_LABEL,
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_TYPE,
#if defined(CROSSFIRE)
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_BAUDRATE,
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_SERIALSTATUS,
#endif
#if defined(CROSSFIRE)
  ITEM_MODEL_SETUP_ARMING_MODE,
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_ARMING_TRIGGER,
#endif
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_CHANNELS,
  ITEM_MODEL_SETUP_EXTERNAL_MODULE_RECEIVER,

  ITEM_VIEW_OPTIONS_LABEL,
  ITEM_VIEW_OPTIONS_RADIO_TAB,
  ITEM_VIEW_OPTIONS_MODEL_TAB,

  ITEM_VIEW_OPTIONS_CURVES,
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

#define MODEL_SETUP_2ND_COLUMN        (LCD_W-17*FW-MENUS_SCROLLBAR_WIDTH-1)
#define MODEL_SETUP_3RD_COLUMN        (MODEL_SETUP_2ND_COLUMN+6*FW)

PACK(struct ModelSetupExpandState {
  uint8_t preflight:1;
  uint8_t throttle:1;
  uint8_t viewOpt:1;
});

static struct ModelSetupExpandState expandState;

static uint8_t PREFLIGHT_ROW(uint8_t value) { return expandState.preflight ? value : HIDDEN_ROW; }

static uint8_t THROTTLE_ROW(uint8_t value) { return expandState.throttle ? value : HIDDEN_ROW; }

static uint8_t VIEWOPT_ROW(uint8_t value) { return expandState.viewOpt ? value : HIDDEN_ROW; }

void copySelection(char * dst, const char * src, uint8_t size)
{
  if (memcmp(src, "---", 3) == 0)
    memset(dst, 0, size);
  else
    memcpy(dst, src, size);
}

void onModelSetupBitmapMenu(const char * result)
{
  if (result == STR_UPDATE_LIST) {
    if (!sdListFiles(BITMAPS_PATH, BITMAPS_EXT, LEN_BITMAP_NAME, nullptr)) {
      POPUP_WARNING(STR_NO_BITMAPS_ON_SD);
    }
  }
  else if (result != STR_EXIT) {
    // The user choosed a bmp file in the list
    copySelection(g_model.header.bitmap, result, LEN_BITMAP_NAME);
    memcpy(modelHeaders[g_eeGeneral.currModel].bitmap, g_model.header.bitmap, LEN_BITMAP_NAME);
    storageDirty(EE_MODEL);
  }
}

void editTimerMode(int timerIdx, coord_t y, LcdFlags attr, event_t event)
{
  TimerData &timer = g_model.timers[timerIdx];
  drawStringWithIndex(0 * FW, y, STR_TIMER, timerIdx + 1);

  lcdDrawTextAtIndex(MODEL_SETUP_2ND_COLUMN, y, STR_VTMRMODES, timer.mode,
                     menuHorizontalPosition == 0 ? attr : 0);

  drawSwitch(MODEL_SETUP_3RD_COLUMN, y, timer.swtch,
             menuHorizontalPosition == 1 ? attr : 0);

  // drawTimer(MODEL_SETUP_3RD_COLUMN, y, timer.start,
  //           menuHorizontalPosition == 1 ? attr | TIMEHOUR : TIMEHOUR,
  //           menuHorizontalPosition == 2 ? attr | TIMEHOUR : TIMEHOUR);

  if (attr && menuHorizontalPosition < 0) {
    lcdDrawFilledRect(MODEL_SETUP_2ND_COLUMN - 1, y - 1, 10 * FW, FH + 1);
  }

  if (attr && s_editMode > 0) {
    switch (menuHorizontalPosition) {
      case 0:
        CHECK_INCDEC_MODELVAR_ZERO(event, timer.mode, TMRMODE_MAX);
        break;
      case 1:
        CHECK_INCDEC_MODELSWITCH(event, timer.swtch, SWSRC_FIRST_IN_MIXES,
                                 SWSRC_LAST_IN_MIXES, isSwitchAvailableInMixes);
        break;
    }
  }
}

void editTimerStart(int timerIdx, coord_t y, LcdFlags attr, event_t event)
{
  lcdDrawTextIndented(y, STR_START);

  TimerData* timer = &(g_model.timers[timerIdx]);

  drawTimer(MODEL_SETUP_2ND_COLUMN, y, timer->start,
            menuHorizontalPosition == 0 ? attr : 0,
            menuHorizontalPosition == 1 ? attr : 0);

  if (g_model.timers[timerIdx].start) {
    lcdDrawTextAtIndex(MODEL_SETUP_3RD_COLUMN, y, STR_TIMER_DIR,
                       g_model.timers[timerIdx].showElapsed,
                       menuHorizontalPosition == 2 ? attr : 0);
  }

  if (attr && menuHorizontalPosition < 0) {
    lcdDrawFilledRect(MODEL_SETUP_2ND_COLUMN - 1, y - 1, 4 * FW, FH + 1);
  }

  if (attr && s_editMode > 0) {
    div_t qr = div(timer->start, 60);
    switch (menuHorizontalPosition) {
      case 0:
        CHECK_INCDEC_MODELVAR_ZERO(event, qr.quot, 539);  // 8:59
        timer->start = qr.rem + qr.quot * 60;
        break;
      case 1:
        qr.rem -= checkIncDecModel(event, qr.rem + 2, 1, 62) - 2;
        timer->start -= qr.rem;
        if ((int16_t)timer->start < 0) timer->start = 0;
        if ((int16_t)timer->start > 5999) timer->start = 32399;  // 8:59:59
        break;
      case 2:
        if (g_model.timers[timerIdx].start) {
            g_model.timers[timerIdx].showElapsed = checkIncDecModel(
                event, g_model.timers[timerIdx].showElapsed, 0, 1);
        }
        break;
    }
  }
}

void editTimerCountdown(int timerIdx, coord_t y, LcdFlags attr, event_t event)
{
  TimerData & timer = g_model.timers[timerIdx];
  lcdDrawTextIndented(y, STR_BEEPCOUNTDOWN);
  int value = timer.countdownBeep;
  if (timer.extraHaptic) value += (COUNTDOWN_NON_HAPTIC_LAST + 1);
  lcdDrawTextAtIndex(MODEL_SETUP_2ND_COLUMN, y, STR_VBEEPCOUNTDOWN, value,
                     (menuHorizontalPosition == 0 ? attr : 0));
  if (timer.countdownBeep != COUNTDOWN_SILENT) {
    lcdDrawNumber(MODEL_SETUP_3RD_COLUMN + 8 * FW, y, TIMER_COUNTDOWN_START(timerIdx), (menuHorizontalPosition == 1 ? attr : 0) | LEFT);
    lcdDrawChar(lcdLastRightPos, y, 's');
  }
  if (attr && s_editMode>0) {
    switch (menuHorizontalPosition) {
      case 0:
      {
        value = timer.countdownBeep;
        if (timer.extraHaptic) value += (COUNTDOWN_NON_HAPTIC_LAST + 1);
        TRACE("value=%d\ttimer.extraHaptic=%d", value, timer.extraHaptic);
        CHECK_INCDEC_MODELVAR(event, value, COUNTDOWN_SILENT, COUNTDOWN_COUNT - 1);
        if (value > COUNTDOWN_VOICE + 1) {
          timer.extraHaptic = 1;
          timer.countdownBeep = value - (COUNTDOWN_NON_HAPTIC_LAST + 1);
        } else {
          timer.extraHaptic = 0;
          timer.countdownBeep = value;
        }
      }
      break;
      case 1:
        timer.countdownStart = -checkIncDecModel(event, -timer.countdownStart, -1, +2);
        break;
    }
  }
}

#define IF_INTERNAL_MODULE_ON(x)          (IS_INTERNAL_MODULE_ENABLED() ? (uint8_t)(x) : HIDDEN_ROW)
#define IF_EXTERNAL_MODULE_ON(x)          (IS_EXTERNAL_MODULE_ENABLED() ? (uint8_t)(x) : HIDDEN_ROW)

inline uint8_t TIMER_ROW(uint8_t timer, uint8_t value)
{
  if (g_model.timers[timer].mode > 0)
    return value;
  return HIDDEN_ROW;
}

#define TIMER_ROWS(x)                                                  \
  1 | NAVIGATION_LINE_BY_LINE, TIMER_ROW(x,0),                         \
      TIMER_ROW(x,(uint8_t)((g_model.timers[x].start) ? 2 : 1) | NAVIGATION_LINE_BY_LINE),       \
      TIMER_ROW(x,0), TIMER_ROW(x,0),                                  \
      TIMER_ROW(x,g_model.timers[x].countdownBeep != COUNTDOWN_SILENT ? (uint8_t)1 : (uint8_t)0)

#if TIMERS == 1
#define TIMERS_ROWS                       TIMER_ROWS(0)
#elif TIMERS == 2
#define TIMERS_ROWS                       TIMER_ROWS(0), TIMER_ROWS(1)
#elif TIMERS == 3
#define TIMERS_ROWS                       TIMER_ROWS(0), TIMER_ROWS(1), TIMER_ROWS(2)
#endif

#if defined(PCBX9E)
  #define SW_WARN_ROWS \
    PREFLIGHT_ROW(uint8_t(NAVIGATION_LINE_BY_LINE|(getSwitchWarningsCount()-1))), \
    PREFLIGHT_ROW(uint8_t(getSwitchWarningsCount() > 8 ? TITLE_ROW : HIDDEN_ROW)), \
    PREFLIGHT_ROW(uint8_t(getSwitchWarningsCount() > 16 ? TITLE_ROW : HIDDEN_ROW))
  #define POT_WARN_ROWS \
    PREFLIGHT_ROW(uint8_t(g_model.potsWarnMode ? NAVIGATION_LINE_BY_LINE|(MAX_POTS) : 0)), \
    PREFLIGHT_ROW(uint8_t(g_model.potsWarnMode ? TITLE_ROW : HIDDEN_ROW))
  #define TOPLCD_ROWS                     0,
#else
  #define SW_WARN_ROWS \
    PREFLIGHT_ROW(uint8_t(NAVIGATION_LINE_BY_LINE|getSwitchWarningsCount()))
  #define POT_WARN_ROWS \
    PREFLIGHT_ROW(uint8_t(g_model.potsWarnMode ? NAVIGATION_LINE_BY_LINE|(MAX_POTS) : 0))
  #define TOPLCD_ROWS
#endif
#if defined(CROSSFIRE)
#define IF_MODULE_SYNCED(module, xxx)        (isModuleCrossfire(module) ? (uint8_t)(xxx) : HIDDEN_ROW)
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

#define CURRENT_MODULE_EDITED(k)      (k >= ITEM_MODEL_SETUP_EXTERNAL_MODULE_LABEL ? EXTERNAL_MODULE : INTERNAL_MODULE)

#include "model_setup_crsf.h"

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

uint8_t viewOptChoice(coord_t y, const char* title, uint8_t value, uint8_t attr, event_t event, bool globalState)
{
  lcdDrawText(INDENT_WIDTH*2, y, title);
  uint8_t rv = editChoice(MODEL_SETUP_2ND_COLUMN, y, nullptr, STR_ADCFILTERVALUES, value, 0, 2, attr, event);
  if (rv == OVERRIDE_GLOBAL)
    lcdDrawText(MODEL_SETUP_2ND_COLUMN + 40, y, STR_ADCFILTERVALUES[globalState == 0 ? 2 : 1]);
  return rv;
}

void menuModelSetup(event_t event)
{
  horzpos_t l_posHorz = menuHorizontalPosition;
  bool CURSOR_ON_CELL = (menuHorizontalPosition >= 0);

  int8_t old_editMode = s_editMode;

  MENU_TAB({
    HEADER_LINE_COLUMNS

    0, // ITEM_MODEL_SETUP_NAME
    0, // ITEM_MODEL_SETUP_BITMAP

    TIMERS_ROWS,

    TOPLCD_ROWS

    0, // ITEM_MODEL_SETUP_EXTENDED_LIMITS

    0, // ITEM_MODEL_SETUP_THROTTLE_LABEL
    THROTTLE_ROW(0), // ITEM_MODEL_SETUP_THROTTLE_REVERSED
    THROTTLE_ROW(0), // ITEM_MODEL_SETUP_THROTTLE_TRACE

    0,   // ITEM_MODEL_SETUP_PREFLIGHT_LABEL
      PREFLIGHT_ROW(0), // ITEM_MODEL_SETUP_CHECKLIST_DISPLAY
      PREFLIGHT_ROW(g_model.displayChecklist ? 0 : HIDDEN_ROW), // Checklist interactive
      PREFLIGHT_ROW(0), // Throttle warning
      PREFLIGHT_ROW(!g_model.disableThrottleWarning ? 0 : HIDDEN_ROW), // Custom position for throttle warning enable
      PREFLIGHT_ROW(!g_model.disableThrottleWarning && g_model.enableCustomThrottleWarning ? 0 : HIDDEN_ROW), // Custom position for throttle warning value
      SW_WARN_ROWS, // ITEM_MODEL_SETUP_SWITCHES_WARNING1
      POT_WARN_ROWS, // ITEM_MODEL_SETUP_POTS_WARNING

    uint8_t(NAVIGATION_LINE_BY_LINE | (adcGetInputOffset(ADC_INPUT_FLEX + 1) - 1)), // ITEM_MODEL_SETUP_BEEP_CENTER

    0, // ITEM_MODEL_SETUP_USE_JITTER_FILTER

    LABEL(InternalModule), 0,
      MODULE_CHANNELS_ROWS(INTERNAL_MODULE),
      MODULE_BIND_ROWS(INTERNAL_MODULE),
    LABEL(ExternalModule), 0,
      IF_MODULE_BAUDRATE_ADJUST(EXTERNAL_MODULE, 0),
      IF_MODULE_SYNCED(EXTERNAL_MODULE, 0),
      IF_MODULE_ARMED(EXTERNAL_MODULE, 0),
      IF_MODULE_ARMED_TRIGGER(EXTERNAL_MODULE, 0),
      MODULE_CHANNELS_ROWS(EXTERNAL_MODULE),
      MODULE_BIND_ROWS(EXTERNAL_MODULE),

    // View options
    0,
     VIEWOPT_ROW(LABEL(RadioMenuTabs)),

      VIEWOPT_ROW(LABEL(ModelMenuTabs)),

      VIEWOPT_ROW(0),

      CASE_LUA_MODEL_SCRIPTS(VIEWOPT_ROW(0))
      VIEWOPT_ROW(0),

    USB_JOYSTICK_ROWS
  });

  MENU_CHECK(menuTabModel, MENU_MODEL_SETUP, ITEM_MODEL_SETUP_LINES_COUNT);
  title(STR_MENU_MODEL_SETUP);

  if (event == EVT_ENTRY || event == EVT_ENTRY_UP) {
    memclear(&reusableBuffer.moduleSetup, sizeof(reusableBuffer.moduleSetup));
  }

  int sub = menuVerticalPosition;

  for (int i = 0; i < NUM_BODY_LINES; ++i) {
    coord_t y = MENU_HEADER_HEIGHT + 1 + i*FH;
    uint8_t k = i + menuVerticalOffset;
    for (int j = 0; j <= k; j++) {
      if (mstate_tab[j] == HIDDEN_ROW)
        k++;
    }

    LcdFlags blink = ((s_editMode>0) ? BLINK|INVERS : INVERS);
    LcdFlags attr = (sub == k ? blink : 0);

    switch (k) {
      case ITEM_MODEL_SETUP_NAME:
        editSingleName(MODEL_SETUP_2ND_COLUMN, y, STR_MODELNAME,
                       g_model.header.name, sizeof(g_model.header.name), event,
                       attr, old_editMode);
        memcpy(modelHeaders[g_eeGeneral.currModel].name, g_model.header.name,
               sizeof(g_model.header.name));
        break;

      case ITEM_MODEL_SETUP_BITMAP:
        lcdDrawTextAlignedLeft(y, STR_BITMAP);
        if (ZEXIST(g_model.header.bitmap))
          lcdDrawSizedText(MODEL_SETUP_2ND_COLUMN, y, g_model.header.bitmap, LEN_BITMAP_NAME, attr);
        else
          lcdDrawText(MODEL_SETUP_2ND_COLUMN, y, STR_EMPTY, attr);
        if (attr && event==EVT_KEY_BREAK(KEY_ENTER)) {
          s_editMode = 0;
          if (sdListFiles(BITMAPS_PATH, BITMAPS_EXT, LEN_BITMAP_NAME, g_model.header.bitmap, LIST_NONE_SD_FILE)) {
            POPUP_MENU_START(onModelSetupBitmapMenu);
          }
          else {
            POPUP_WARNING(STR_NO_BITMAPS_ON_SD);
          }
        }
        break;

      case ITEM_MODEL_SETUP_TIMER1:
        editTimerMode(0, y, attr, event);
        break;

      case ITEM_MODEL_SETUP_TIMER1_NAME:
        editSingleName(MODEL_SETUP_2ND_COLUMN, y, STR_NAME,
                       g_model.timers[0].name, LEN_TIMER_NAME, event, attr,
                       old_editMode, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_TIMER1_START:
        editTimerStart(0, y, attr, event);
        break;

          case ITEM_MODEL_SETUP_TIMER1_MINUTE_BEEP:
            g_model.timers[0].minuteBeep = editCheckBox(
                g_model.timers[0].minuteBeep, MODEL_SETUP_2ND_COLUMN, y,
                STR_MINUTEBEEP, attr, event, INDENT_WIDTH);
            break;

          case ITEM_MODEL_SETUP_TIMER1_COUNTDOWN_BEEP:
            editTimerCountdown(0, y, attr, event);
            break;

          case ITEM_MODEL_SETUP_TIMER1_PERSISTENT:
            g_model.timers[0].persistent = editChoice(
                MODEL_SETUP_2ND_COLUMN, y, STR_PERSISTENT, STR_VPERSISTENT,
                g_model.timers[0].persistent, 0, 2, attr, event, INDENT_WIDTH);
            break;

#if TIMERS > 1
      case ITEM_MODEL_SETUP_TIMER2:
        editTimerMode(1, y, attr, event);
        break;

      case ITEM_MODEL_SETUP_TIMER2_NAME:
        editSingleName(MODEL_SETUP_2ND_COLUMN, y, STR_NAME,
                       g_model.timers[1].name, LEN_TIMER_NAME, event, attr,
                       old_editMode, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_TIMER2_START:
        editTimerStart(1, y, attr, event);
        break;

      case ITEM_MODEL_SETUP_TIMER2_MINUTE_BEEP:
        g_model.timers[1].minuteBeep = editCheckBox(g_model.timers[1].minuteBeep, MODEL_SETUP_2ND_COLUMN, y, STR_MINUTEBEEP, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_TIMER2_COUNTDOWN_BEEP:
        editTimerCountdown(1, y, attr, event);
        break;

      case ITEM_MODEL_SETUP_TIMER2_PERSISTENT:
        g_model.timers[1].persistent = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_PERSISTENT, STR_VPERSISTENT, g_model.timers[1].persistent, 0, 2, attr, event, INDENT_WIDTH);
        break;
#endif

#if TIMERS > 2
      case ITEM_MODEL_SETUP_TIMER3:
        editTimerMode(2, y, attr, event);
        break;

      case ITEM_MODEL_SETUP_TIMER3_NAME:
        editSingleName(MODEL_SETUP_2ND_COLUMN, y, STR_NAME,
                       g_model.timers[2].name, LEN_TIMER_NAME, event, attr,
                       old_editMode, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_TIMER3_START:
        editTimerStart(2, y, attr, event);
        break;

      case ITEM_MODEL_SETUP_TIMER3_MINUTE_BEEP:
        g_model.timers[2].minuteBeep =
            editCheckBox(g_model.timers[2].minuteBeep, MODEL_SETUP_2ND_COLUMN,
                         y, STR_MINUTEBEEP, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_TIMER3_COUNTDOWN_BEEP:
        editTimerCountdown(2, y, attr, event);
        break;

      case ITEM_MODEL_SETUP_TIMER3_PERSISTENT:
        g_model.timers[2].persistent = editChoice(
            MODEL_SETUP_2ND_COLUMN, y, STR_PERSISTENT, STR_VPERSISTENT,
            g_model.timers[2].persistent, 0, 2, attr, event, INDENT_WIDTH);
        break;
#endif

#if defined(PCBX9E)
      case ITEM_MODEL_SETUP_TOP_LCD_TIMER:
        lcdDrawTextAlignedLeft(y, STR_TOPLCDTIMER);
        drawStringWithIndex(MODEL_SETUP_2ND_COLUMN, y, STR_TIMER, g_model.toplcdTimer+1, attr);
        if (attr) {
          g_model.toplcdTimer = checkIncDec(event, g_model.toplcdTimer, 0, TIMERS-1, EE_MODEL);
        }
        break;
#endif

      case ITEM_MODEL_SETUP_EXTENDED_LIMITS:
        g_model.extendedLimits = editCheckBox(g_model.extendedLimits, MODEL_SETUP_2ND_COLUMN, y, STR_ELIMITS, attr, event);
        break;

      case ITEM_MODEL_SETUP_THROTTLE_LABEL:
        expandState.throttle = expandableSection(y, STR_THROTTLE_LABEL, expandState.throttle, attr, event);
        break;

      case ITEM_MODEL_SETUP_THROTTLE_REVERSED:
        g_model.throttleReversed = editCheckBox(g_model.throttleReversed, MODEL_SETUP_2ND_COLUMN, y, STR_THROTTLEREVERSE, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_THROTTLE_TRACE:
      {
        lcdDrawTextIndented(y, STR_TTRACE);
        if (attr)
          CHECK_INCDEC_MODELVAR_ZERO_CHECK(
              event, g_model.thrTraceSrc,
              MAX_POTS + MAX_OUTPUT_CHANNELS,
              isThrottleSourceAvailable);

        uint8_t idx = throttleSource2Source(g_model.thrTraceSrc);
        drawSource(MODEL_SETUP_2ND_COLUMN, y, idx, attr);
        break;
      }

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
        {
          lcdDrawText(INDENT_WIDTH * 4, y, STR_CUSTOM_THROTTLE_WARNING_VAL);
          lcdDrawNumber(MODEL_SETUP_2ND_COLUMN, y, g_model.customThrottleWarningPosition, attr | LEFT, 2);
          if (attr) {
            CHECK_INCDEC_MODELVAR(event, g_model.customThrottleWarningPosition, -100, 100);
          }
        }
        break;

#if defined(PCBX9E)
      case ITEM_MODEL_SETUP_SWITCHES_WARNING2:
      case ITEM_MODEL_SETUP_SWITCHES_WARNING3:
      case ITEM_MODEL_SETUP_POTS_WARNING2:
        if (i==0) {
          if (IS_PREVIOUS_EVENT(event))
            menuVerticalOffset--;
          else
            menuVerticalOffset++;
        }
        break;
#endif

      case ITEM_MODEL_SETUP_SWITCHES_WARNING1:
      {
#if defined(PCBX9E)
        if (i>=NUM_BODY_LINES-2 && getSwitchWarningsCount() > 8*(NUM_BODY_LINES-i)) {
          if (IS_PREVIOUS_EVENT(event))
            menuVerticalOffset--;
          else
            menuVerticalOffset++;
          break;
        }
#endif
        lcdDrawTextIndented(y, STR_SWITCHWARNING);

        if (attr) {
          s_editMode = 0;
          switch (event) {
            case EVT_KEY_LONG(KEY_ENTER):
              killEvents(event);
              if (menuHorizontalPosition < 0) {
                START_NO_HIGHLIGHT();
                setAllPreflightSwitchStates();
              }
              break;
          }
        }

        LcdFlags line = attr;

        int current = 0;
        for (int i = 0; i < switchGetMaxAllSwitches(); i++) {
          if (SWITCH_WARNING_ALLOWED(i)) {
            div_t qr = div(current, 8);
            if (event == EVT_KEY_BREAK(KEY_ENTER) && line &&
                l_posHorz == current) {
              uint8_t curr_state = g_model.getSwitchWarning(i);
              // add the new one (if switch UP and 2POS, jump directly to DOWN)
              curr_state += (curr_state != 1 || IS_CONFIG_3POS(i) ? 1 : 2);
              g_model.setSwitchWarning(i, curr_state);
              storageDirty(EE_MODEL);
            }
            lcdDrawChar(
                MODEL_SETUP_2ND_COLUMN + qr.rem * (2 * FW + 1),
                y + FH * qr.quot, 'A' + i,
                line && (menuHorizontalPosition == current) ? INVERS : 0);
            lcdDrawText(lcdNextPos, y + FH * qr.quot,
                        getSwitchWarnSymbol(g_model.getSwitchWarning(i)));
            ++current;
          }
        }
        if (attr && menuHorizontalPosition < 0) {
#if defined(PCBX9E)
          lcdDrawFilledRect(MODEL_SETUP_2ND_COLUMN-1, y-1, 8*(2*FW+1), 1+FH*((current+7)/8));
#else
          lcdDrawFilledRect(MODEL_SETUP_2ND_COLUMN-1, y-1, current*(2*FW+1), FH+1);
#endif
        }
        break;
      }

      case ITEM_MODEL_SETUP_POTS_WARNING:
#if defined(PCBX9E)
        if (i==NUM_BODY_LINES-1 && g_model.potsWarnMode) {
          if (IS_PREVIOUS_EVENT(event))
            menuVerticalOffset--;
          else
            menuVerticalOffset++;
          break;
        }
#endif

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
              if (max_pots > 5 && i == 3) {
                y += FH;
                x = MODEL_SETUP_2ND_COLUMN;
              }
              LcdFlags flags = ((menuHorizontalPosition==i+1) && attr) ? BLINK : 0;
              if ((!attr || menuHorizontalPosition >= 0) &&
                  (g_model.potsWarnEnabled & (1 << i))) {
                flags |= INVERS;
              }

              lcdDrawText(x, y, getPotLabel(i), flags);
              x = lcdNextPos+3;
            }
          }
        }
        if (attr && menuHorizontalPosition < 0) {
#if defined(PCBX9E)
          lcdDrawFilledRect(MODEL_SETUP_2ND_COLUMN-1, y-FH-1, LCD_W-MODEL_SETUP_2ND_COLUMN-MENUS_SCROLLBAR_WIDTH+1, 2*FH+1);
#else
          lcdDrawFilledRect(MODEL_SETUP_2ND_COLUMN-1, y-1, LCD_W-MODEL_SETUP_2ND_COLUMN-MENUS_SCROLLBAR_WIDTH+1, FH+1);
#endif
        }
        break;

      case ITEM_MODEL_SETUP_BEEP_CENTER: {
        lcdDrawTextAlignedLeft(y, STR_BEEPCTR);
        uint8_t pot_offset = adcGetInputOffset(ADC_INPUT_FLEX);
        uint8_t max_inputs = adcGetMaxInputs(ADC_INPUT_MAIN) + adcGetMaxInputs(ADC_INPUT_FLEX);
        coord_t x = MODEL_SETUP_2ND_COLUMN;
        for (uint8_t i = 0; i < max_inputs; i++) {
          if ( i >= pot_offset && (IS_POT_MULTIPOS(i - pot_offset) || !IS_POT_SLIDER_AVAILABLE(i - pot_offset)) ) {
            if (attr && menuHorizontalPosition == i) repeatLastCursorMove(event);
            continue;
          }
          LcdFlags flags = 0;
          if ((menuHorizontalPosition == i) && attr)
            flags = BLINK | INVERS;
          else if (ANALOG_CENTER_BEEP(i) || (attr && CURSOR_ON_LINE()))
            flags = INVERS;
          if (adcGetMaxInputs(ADC_INPUT_FLEX) > 4 || i < pot_offset) {
            lcdDrawText(x, y, getAnalogShortLabel(i), flags);
          }
          else {
            lcdDrawText(x, y, getPotLabel(i - pot_offset), flags);
          }
          x = lcdNextPos;
          if (i >= pot_offset - 1) x+=2;
        }
        if (attr && CURSOR_ON_CELL) {
          if (event==EVT_KEY_BREAK(KEY_ENTER)) {
            s_editMode = 0;
            g_model.beepANACenter ^= ((BeepANACenter)1<<menuHorizontalPosition);
            storageDirty(EE_MODEL);
          }
        }
        break;
      }

      case ITEM_MODEL_SETUP_USE_JITTER_FILTER:
        g_model.jitterFilter = editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_JITTER_FILTER, STR_ADCFILTERVALUES, g_model.jitterFilter, 0, 2, attr, event);
        break;

      case ITEM_MODEL_SETUP_INTERNAL_MODULE_LABEL:
        lcdDrawTextAlignedLeft(y, STR_INTERNALRF);
        break;

      case ITEM_MODEL_SETUP_INTERNAL_MODULE_TYPE:
              editCrsfModuleType(INTERNAL_MODULE, y, attr, event);
        break;

      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_LABEL:
        lcdDrawTextAlignedLeft(y, STR_EXTERNALRF);
        break;

      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_TYPE:
        editCrsfModuleType(EXTERNAL_MODULE, y, attr, event);
        break;

#if defined(CROSSFIRE)
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_BAUDRATE: {
        ModuleData &moduleData = g_model.moduleData[EXTERNAL_MODULE];
        lcdDrawTextIndented(y, STR_BAUDRATE);
        if (isModuleCrossfire(EXTERNAL_MODULE)) {
          lcdDrawTextAtIndex(MODEL_SETUP_2ND_COLUMN, y, STR_CRSF_BAUDRATE, CROSSFIRE_STORE_TO_INDEX(moduleData.crsf.telemetryBaudrate),attr | LEFT);
          if (attr) {
            moduleData.crsf.telemetryBaudrate =CROSSFIRE_INDEX_TO_STORE(checkIncDecModel(event,CROSSFIRE_STORE_TO_INDEX(moduleData.crsf.telemetryBaudrate),0, DIM(CROSSFIRE_BAUDRATES) - 1));
            if (checkIncDec_Ret) {
              restartModule(EXTERNAL_MODULE);
            }
          }
        }
        break;

      }

      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_SERIALSTATUS:
        lcdDrawTextIndented(y, STR_STATUS);
        lcdDrawNumber(MODEL_SETUP_2ND_COLUMN, y, 1000000 / getMixerSchedulerPeriod(), LEFT | attr);
        lcdDrawText(lcdNextPos, y, "Hz ", attr);
        break;
#endif

#if defined(CROSSFIRE)
      case ITEM_MODEL_SETUP_ARMING_MODE:
        g_model.moduleData[EXTERNAL_MODULE].crsf.crsfArmingMode =
          editChoice(MODEL_SETUP_2ND_COLUMN, y, STR_CRSF_ARMING_MODE, STR_CRSF_ARMING_MODES,
          g_model.moduleData[EXTERNAL_MODULE].crsf.crsfArmingMode, ARMING_MODE_FIRST, ARMING_MODE_LAST, attr, event, INDENT_WIDTH);
        break;

      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_ARMING_TRIGGER:
        lcdDrawTextIndented(y, STR_SWITCH);
        drawSwitch(MODEL_SETUP_2ND_COLUMN, y, g_model.moduleData[EXTERNAL_MODULE].crsf.crsfArmingTrigger, attr);
        if(attr)
          CHECK_INCDEC_SWITCH(event, g_model.moduleData[EXTERNAL_MODULE].crsf.crsfArmingTrigger, SWSRC_FIRST, SWSRC_LAST, EE_MODEL, isSwitchAvailableForArming);
        break;
#endif

      case ITEM_MODEL_SETUP_INTERNAL_MODULE_CHANNELS:
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_CHANNELS:
            {
        uint8_t moduleIdx = CURRENT_MODULE_EDITED(k);
        editCrsfChannels(moduleIdx, y, attr, event);
        break;
      }

      case ITEM_MODEL_SETUP_INTERNAL_MODULE_RECEIVER:
      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_RECEIVER:
            {
        uint8_t moduleIdx = CURRENT_MODULE_EDITED(k);
        editCrsfReceiver(moduleIdx, y, attr, event);
        break;
      }

      case ITEM_VIEW_OPTIONS_LABEL:
        expandState.viewOpt = expandableSection(y, STR_ENABLED_FEATURES, expandState.viewOpt, attr, event);
        break;
      case ITEM_VIEW_OPTIONS_RADIO_TAB:
        lcdDrawTextIndented(y, STR_RADIO_MENU_TABS);
        break;

      case ITEM_VIEW_OPTIONS_MODEL_TAB:
        lcdDrawTextIndented(y, STR_MODEL_MENU_TABS);
        break;
      case ITEM_VIEW_OPTIONS_CURVES:
        g_model.modelCurvesDisabled = viewOptChoice(y, STR_MENUCURVES, g_model.modelCurvesDisabled, attr, event, g_eeGeneral.modelCurvesDisabled);
        break;
#if defined(LUA_MODEL_SCRIPTS)
      case ITEM_VIEW_OPTIONS_CUSTOM_SCRIPTS:
        g_model.modelCustomScriptsDisabled = viewOptChoice(y, STR_MENUCUSTOMSCRIPTS, g_model.modelCustomScriptsDisabled, attr, event, g_eeGeneral.modelCustomScriptsDisabled);
        break;
#endif
      case ITEM_VIEW_OPTIONS_TELEMETRY:
        g_model.modelTelemetryDisabled = viewOptChoice(y, STR_MENUTELEMETRY, g_model.modelTelemetryDisabled, attr, event, g_eeGeneral.modelTelemetryDisabled);
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

  if (old_editMode > 0 && s_editMode == 0) {
    switch(menuVerticalPosition) {
      case ITEM_MODEL_SETUP_INTERNAL_MODULE_RECEIVER:
        if (menuHorizontalPosition == 0)
          checkModelIdUnique(g_eeGeneral.currModel, INTERNAL_MODULE);
        break;

      case ITEM_MODEL_SETUP_EXTERNAL_MODULE_RECEIVER:
        if (menuHorizontalPosition == 0)
          checkModelIdUnique(g_eeGeneral.currModel, EXTERNAL_MODULE);
        break;
    }
  }
}
