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
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"

#include "switches.h"
#include "input_mapping.h"

#define BIGSIZE       DBLSIZE
#if defined (PCBTARANIS)
  #define LBOX_CENTERX  (LCD_W/4 + 12)
  #define RBOX_CENTERX  (3*LCD_W/4 - 13)
#else
  #define LBOX_CENTERX  (LCD_W/4 + 10)
  #define RBOX_CENTERX  (3*LCD_W/4 - 10)
#endif
#define MODELNAME_X   (2*FW-2)
#define MODELNAME_Y   (0)
#define VBATT_X       (6*FW-1)
#define VBATT_Y       (2*FH)
#define VBATTUNIT_Y   (3*FH)
#define REBOOT_X      (2)
#define BAR_HEIGHT    (BOX_WIDTH-1l) // don't remove the l here to force 16bits maths on 9X
#define RSSSI_X       (30)
#define RSSSI_Y       (31)
#define RSSI_MAX      105
#define CLOCK_X       53
#define CLOCK_Y       57

#if defined(ASTERISK) || (!defined(USE_WATCHDOG) && !defined(SIMU)) || defined(LOG_TELEMETRY) || \
    defined(LOG_BLUETOOTH) || defined(DEBUG_LATENCY)

static bool isAsteriskDisplayed() {
  return true;
}

#else

#include "hal/abnormal_reboot.h"

static bool isAsteriskDisplayed() {
  return UNEXPECTED_SHUTDOWN();
}
#endif

void drawExternalAntennaAndRSSI()
{
#if defined(EXTERNAL_ANTENNA)
  if (isExternalAntennaEnabled()) {
    lcdDrawText(VBATT_X - 1, VBATT_Y + 8, "E", TINSIZE);
  }
#endif

  if (TELEMETRY_RSSI() > 0) {
    auto warningRSSI = g_model.rfAlarms.warning;
    int8_t value = TELEMETRY_RSSI() - warningRSSI;
    uint8_t step = (RSSI_MAX - warningRSSI) / 4;
    for (uint8_t i = 1; i < 5; i++) {
      if (value > step * (i - 1)) {
        lcdDrawFilledRect(RSSSI_X + i * 4, RSSSI_Y - 2 * i + 1, 3, 2 * i - 1, SOLID, 0);
      }
    }
  }
}

void drawPotsBars()
{
  uint8_t max_pots = adcGetMaxInputs(ADC_INPUT_FLEX);
  uint8_t offset = adcGetInputOffset(ADC_INPUT_FLEX);
  uint8_t configured_pots = 0;

  for (uint8_t i = 0; i < max_pots; i++) {
    if (IS_POT_SLIDER_AVAILABLE(i)) {
      configured_pots ++;
    }
  }

  uint8_t lines = configured_pots > 3 ? 2 : 1;
  uint8_t cols =  configured_pots > 4 ? 3 : configured_pots % 2 ? 3 : 2;
  coord_t xstart =  LCD_W / 2 - (cols % 2 ? 5 : 3);

  for (uint8_t i = 0, j = 0; i < max_pots; i++) {
    if (IS_POT_SLIDER_AVAILABLE(i)) {
      coord_t x = xstart + (j % cols) * 5;
      coord_t y = lines == 1 ? (LCD_H - 8) : j >= cols ? (LCD_H - 8) : (LCD_H - 8 - BAR_HEIGHT / 2 - 1);
      auto v = calibratedAnalogs[offset + i] + RESX;
      uint8_t len = (v * (BAR_HEIGHT - (lines - 1)) / (RESX * 2 * lines)) + 1l;
      V_BAR(x, y, len);
      j++;
    }
  }
}

void doMainScreenGraphics()
{
  int16_t calibStickVert = calibratedAnalogs[ADC_MAIN_LV];
  if (g_model.throttleReversed &&
      inputMappingConvertMode(ADC_MAIN_LV) == inputMappingGetThrottle()) {
    calibStickVert = -calibStickVert;
  }
  drawStick(LBOX_CENTERX, calibratedAnalogs[ADC_MAIN_LH], calibStickVert);

  calibStickVert = calibratedAnalogs[ADC_MAIN_RV];
  if (g_model.throttleReversed &&
      inputMappingConvertMode(ADC_MAIN_RV) == inputMappingGetThrottle()) {
    calibStickVert = -calibStickVert;
  }
  drawStick(RBOX_CENTERX, calibratedAnalogs[ADC_MAIN_RH], calibStickVert);

  drawPotsBars();
}

void displayBattVoltage()
{
#if defined(BATTGRAPH)
  putsVBat(VBATT_X - 8, VBATT_Y + 1, RIGHT);
  lcdDrawSolidFilledRect(VBATT_X - 25, VBATT_Y + 9, 21, 5);
  lcdDrawSolidVerticalLine(VBATT_X - 4, VBATT_Y + 10, 3);
  uint8_t count = GET_TXBATT_BARS(20);

  #if defined(HARDWARE_CHARGING_STATE)
  if (IS_CHARGING_STATE()) {
    count = (get_tmr10ms() & 127u) * count / 128;
  }
#endif
  for (uint8_t i = 0; i < count; i += 2) {
#if defined(USB_CHARGER)
    if ((i >= count - 2) && usbChargerLed() && BLINK_ON_PHASE)  // Blink last segment on charge
      continue;
#endif
    lcdDrawSolidVerticalLine(VBATT_X - 24 + i, VBATT_Y + 10, 3);
  }
  if (!IS_TXBATT_WARNING() || BLINK_ON_PHASE)
    lcdDrawSolidFilledRect(VBATT_X - 26, VBATT_Y, 24, 15);
#else
  LcdFlags att = (IS_TXBATT_WARNING() ? BLINK|INVERS : 0) | BIGSIZE;
  putsVBat(VBATT_X-1, VBATT_Y, att|NO_UNIT);
  lcdDrawChar(VBATT_X, VBATTUNIT_Y, 'V');
#endif
}

#define displayVoltageOrAlarm() displayBattVoltage()

void onMainViewMenu(const char * result)
{
  if (result == STR_RESET_TIMER1) {
    timerReset(0);
  }
  else if (result == STR_RESET_TIMER2) {
    timerReset(1);
  }
#if TIMERS > 2
  else if (result == STR_RESET_TIMER3) {
    timerReset(2);
  }
#endif
  else if (result == STR_VIEW_NOTES) {
    pushModelNotes();
  }
  else if (result == STR_RESET_SUBMENU) {
    POPUP_MENU_START(onMainViewMenu, 5, STR_RESET_SESSION, STR_RESET_TIMER1, STR_RESET_TIMER2, STR_RESET_TIMER3, STR_RESET_TELEMETRY);
  }
  else if (result == STR_RESET_TELEMETRY) {
    telemetryReset();
  }
  else if (result == STR_RESET_SESSION) {
    flightReset();
  }
  else if (result == STR_STATISTICS) {
    chainMenu(menuStatisticsView);
  }
  else if (result == STR_ABOUT_US) {
    chainMenu(menuAboutView);
  }
}

void drawSmallSwitch(coord_t x, coord_t y, int width, unsigned int index)
{
  if (SWITCH_EXISTS(index)) {
    int val = getValue(MIXSRC_FIRST_SWITCH + index);

    if (val >= 0) {
      lcdDrawSolidHorizontalLine(x, y, width);
      lcdDrawSolidHorizontalLine(x, y + 2, width);
      y += 4;
      if (val > 0) {
        lcdDrawSolidHorizontalLine(x, y, width);
        lcdDrawSolidHorizontalLine(x, y + 2, width);
        y += 4;
      }
    }

    lcdDrawChar(width == 5 ? x + 1 : x, y, switchGetDefaultName(index)[1], SMLSIZE);
    y += 7;

    if (val <= 0) {
      lcdDrawSolidHorizontalLine(x, y, width);
      lcdDrawSolidHorizontalLine(x, y + 2, width);
      if (val < 0) {
        lcdDrawSolidHorizontalLine(x, y + 4, width);
        lcdDrawSolidHorizontalLine(x, y + 6, width);
      }
    }
  }
}

void menuMainView(event_t event)
{
  uint8_t view = g_eeGeneral.view;
  uint8_t view_base = view & 0x0f;

  switch (event) {
    case EVT_ENTRY:
      killEvents(KEY_EXIT);
      killEvents(KEY_UP);
      killEvents(KEY_DOWN);
      break;

      /* TODO if timer2 is OFF, it's possible to use this timer2 as in er9x...
      case EVT_KEY_BREAK(KEY_MENU):
        if (view_base == VIEW_TIMER2) {
          Timer2_running = !Timer2_running;
          AUDIO_KEY_PRESS();
        }
      break;
      */
    case EVT_KEY_NEXT_PAGE:
    case EVT_KEY_PREVIOUS_PAGE:
      if (view_base == VIEW_INPUTS)
        g_eeGeneral.view ^= ALTERNATE_VIEW;
      else
        g_eeGeneral.view = (g_eeGeneral.view + (4 * ALTERNATE_VIEW) + ((event == EVT_KEY_PREVIOUS_PAGE) ? -ALTERNATE_VIEW : ALTERNATE_VIEW)) % (4 * ALTERNATE_VIEW);
      break;

    case EVT_KEY_CONTEXT_MENU:
      if (modelHasNotes()) {
        POPUP_MENU_ADD_ITEM(STR_VIEW_NOTES);
      }

      POPUP_MENU_START(onMainViewMenu, 3, STR_RESET_SUBMENU, STR_STATISTICS, STR_ABOUT_US);
      break;

#if defined(EVT_KEY_LAST_MENU)
    case EVT_KEY_LAST_MENU:
      pushMenu(lastPopMenu());
      break;
#endif

    case EVT_KEY_MODEL_MENU:
      pushMenu(menuModelSelect);
      break;

    case EVT_KEY_GENERAL_MENU:
      pushMenu(menuTabGeneral[0].menuFunc);
      break;

#if defined(EVT_KEY_PREVIOUS_VIEW)
      // TODO try to split those 2 cases on 9X
    case EVT_KEY_PREVIOUS_VIEW:
    case EVT_KEY_NEXT_VIEW:
      // TODO try to split those 2 cases on 9X
      g_eeGeneral.view = (event == EVT_KEY_PREVIOUS_VIEW ? (view_base == VIEW_COUNT - 1 ? 0 : view_base + 1) : (view_base == 0 ? VIEW_COUNT - 1 : view_base -
                                                                                                                                                  1));
      storageDirty(EE_GENERAL);
      break;
#else
    case EVT_KEY_NEXT_VIEW:
      g_eeGeneral.view = (view_base == 0 ? VIEW_COUNT - 1 : view_base - 1);
      storageDirty(EE_GENERAL);
      break;
#endif

#if defined(EVT_KEY_STATISTICS)
    case EVT_KEY_STATISTICS:
      chainMenu(menuStatisticsView);
      break;
#endif

    case EVT_KEY_TELEMETRY:
      chainMenu(menuViewTelemetry);
      break;

    case EVT_KEY_FIRST(KEY_EXIT):
      break;
  }

  switch (view_base) {
    case VIEW_CHAN_MONITOR:
      menuChannelsViewCommon(event);
      break;

    case VIEW_OUTPUTS_VALUES:
    case VIEW_OUTPUTS_BARS:
      // scroll bar
      lcdDrawHorizontalLine(38, 34, 54, DOTTED);
      lcdDrawSolidHorizontalLine(38 + (g_eeGeneral.view / ALTERNATE_VIEW) * 13, 34, 13, SOLID);
      for (uint8_t i = 0; i < 8; i++) {
        uint8_t x0, y0;
        uint8_t chan = 8 * (g_eeGeneral.view / ALTERNATE_VIEW) + i;
        int16_t val = channelOutputs[chan];

        if (view_base == VIEW_OUTPUTS_VALUES) {
          x0 = (i % 4 * 9 + 3) * FW / 2;
#if LCD_H >= 96
          y0 = i / 4 * FH * 2 + 50;
#else
          y0 = i / 4 * FH + 40;
#endif
          if (g_eeGeneral.ppmunit == PPM_US) {
            lcdDrawNumber(x0 + 4 * FW, y0, PPM_CH_CENTER(chan) + val / 2, RIGHT);
          } else if (g_eeGeneral.ppmunit == PPM_PERCENT_PREC1) {
            lcdDrawNumber(x0 + 4 * FW, y0, calcRESXto1000(val), RIGHT | PREC1);
          } else {
            lcdDrawNumber(x0+4*FW , y0, calcRESXto1000(val)/10, RIGHT); // G: Don't like the decimal part*
          }
        }
        else {
          constexpr coord_t WBAR2 = (50 / 2);
          x0 = i < 4 ? LCD_W / 4 + 2 : LCD_W * 3 / 4 - 2;
#if LCD_H >= 96
          y0 = 45 + (i % 4) * 10;
#else
          y0 = 38 + (i % 4) * 5;
#endif

          const uint16_t lim = (g_model.extendedLimits ? (512 * (long) LIMIT_EXT_PERCENT / 100) : 512) * 2;
          int8_t len = (abs(val) * WBAR2 + lim / 2) / lim;

          if (len > WBAR2)
            len = WBAR2; // prevent bars from going over the end - comment for debugging
          lcdDrawHorizontalLine(x0 - WBAR2, y0, WBAR2 * 2 + 1, DOTTED);
          lcdDrawSolidVerticalLine(x0, y0 - 2, 5);
          if (val > 0)
            x0 += 1;
          else
            x0 -= len;
          lcdDrawSolidHorizontalLine(x0, y0 + 1, len);
          lcdDrawSolidHorizontalLine(x0, y0 - 1, len);
        }
      }
      break;

    case VIEW_TIMER2:
      drawTimerWithMode(87, 5 * FH, 1, RIGHT | DBLSIZE);
      break;

    case VIEW_INPUTS:
      if (view == VIEW_INPUTS) {
        // Sticks + Pots
        doMainScreenGraphics();

        // Switches
        // -> 2 columns: one for each side
        // -> 4 slots on each side
        uint8_t maxSwitch = 0;
        uint8_t leftMaxRow = 0;
        uint8_t rightMaxRow = 0;
        for (uint8_t n = 0; n < switchGetMaxSwitches(); n += 1)
            if (SWITCH_EXISTS(n) && !switchIsFlex(n) && !switchIsCustomSwitch(n)) {
              auto switch_display = switchGetDisplayPosition(n);
              if (switch_display.col) {
                if (switch_display.row > rightMaxRow)
                  rightMaxRow = switch_display.row;
              } else {
                if (switch_display.row > leftMaxRow)
                  leftMaxRow = switch_display.row;
              }
              maxSwitch = n + 1;
            }

        if (leftMaxRow < 3 && rightMaxRow < 3) {
          for (int i = 0; i < maxSwitch; ++i) {
            if (SWITCH_EXISTS(i) && !switchIsFlex(i) && !switchIsCustomSwitch(i)) {
              auto switch_display = switchGetDisplayPosition(i);
              coord_t x = switch_display.col == 0 ? 3 * FW + 3 : 18 * FW + 1;
              coord_t y = 33 + switch_display.row * FH;
              getvalue_t val = getValue(MIXSRC_FIRST_SWITCH + i);
              if (val == 0) x -= 1;
              getvalue_t sw =
                  ((val < 0) ? 3 * i + 1
                              : ((val == 0) ? 3 * i + 2 : 3 * i + 3));
              drawSwitch(x, y, sw, CENTERED, false);
            }
          }
        }
        else {
          for (int i = 0; i < maxSwitch; ++i) {
            if (SWITCH_EXISTS(i) && !switchIsFlex(i) && !switchIsCustomSwitch(i)) {
              auto switch_display = switchGetDisplayPosition(i);
              coord_t x = (switch_display.col == 0 ? 8 : 96) + switch_display.row * 5;
              if (maxSwitch < 9) x += 3;
              drawSmallSwitch(x, 5 * FH + 1, 4, i);
            }
          }
        }
      }
      else {
        // Logical Switches
        uint8_t index = 0;
        uint8_t y = LCD_H - 20;
        for (uint8_t line = 0; line < 2; line++) {
          for (uint8_t column = 0; column < MAX_LOGICAL_SWITCHES / 2; column++) {
            int8_t len = getSwitch(SWSRC_FIRST_LOGICAL_SWITCH + index) ? 10 : 1;
            uint8_t x = (16 + 3 * column);
            lcdDrawSolidVerticalLine(x - 1, y - len, len);
            lcdDrawSolidVerticalLine(x, y - len, len);
            index++;
          }
          y += 12;
        }
      }
      break;
  }

  if (view_base != VIEW_CHAN_MONITOR) {

    // Model Name
    drawModelName(MODELNAME_X, MODELNAME_Y, g_model.header.name, g_eeGeneral.currModel, BIGSIZE);

    // Main Voltage (or alarm if any)
    displayVoltageOrAlarm();

    // Timer 1
    drawTimerWithMode(125, 2 * FH, 0, RIGHT | DBLSIZE);

    // RSSI gauge / external antenna
    drawExternalAntennaAndRSSI();

    // And ! in case of unexpected shutdown
    if (isAsteriskDisplayed()) {
      lcdDrawChar(REBOOT_X, 1, '!', INVERS|BLINK);
    }
  }

#if defined(RTCLOCK)
  if (view_base != VIEW_CHAN_MONITOR && rtcIsValid()) {
    drawRtcTime(CLOCK_X, CLOCK_Y, LEFT|TIMEBLINK);
  }
#endif
}
