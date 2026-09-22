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
#include "hal/usb_driver.h"

#include "switches.h"
#include "input_mapping.h"

#define BIGSIZE       MIDSIZE
#define LBOX_CENTERX  (BOX_WIDTH/2 + 16)
#define RBOX_CENTERX  (LCD_W-LBOX_CENTERX-1)
#define MODELNAME_X   (15)
#define MODELNAME_Y   (11)
#define VBATT_X       (MODELNAME_X+26)
#define VBATT_Y       (FH+3)
#define VBATTUNIT_Y   VBATT_Y
#define BITMAP_X      ((LCD_W-64)/2)
#define BITMAP_Y      (LCD_H/2)
#define TIMERS_X      145
#define TIMERS_Y      20
#define TIMERS_H      25
#define TIMERS_R      193
#define REBOOT_X      (LCD_W-FW)

const unsigned char logo_taranis[]  = {
#include "logo.lbm"
};

const unsigned char icons[]  = {
#include "icons.lbm"
};

#define ICON_RSSI     0, 9
#define ICON_SPEAKER0 9, 8
#define ICON_SPEAKER1 17, 8
#define ICON_SPEAKER2 25, 8
#define ICON_SPEAKER3 33, 8
#define ICON_SD       41, 11
#define ICON_LOGS     51, 11
#define ICON_USB      81, 11
#define ICON_REBOOT   91, 11
#define ICON_ALTITUDE 102, 9

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

void doMainScreenGraphics()
{
  int16_t calibStickVert = calibratedAnalogs[ADC_MAIN_LV];
  drawStick(LBOX_CENTERX, calibratedAnalogs[ADC_MAIN_LH], calibStickVert);

  calibStickVert = calibratedAnalogs[ADC_MAIN_RV];
  drawStick(RBOX_CENTERX, calibratedAnalogs[ADC_MAIN_RH], calibStickVert);
}

// Pots & sliders
// X9E: only sliders (1 to 4)
// Other: POT1, POT2, SLIDERS1 and SLIDER2
//
static const coord_t _pot_slots[] = {
  3, LCD_H / 2 + 1,         // SLIDER1 (x,y)
  LCD_W - 5, LCD_H / 2 + 1, // SLIDER2 (x,y)
  3, 1,                     // SLIDER3 (x,y)
  LCD_W - 5, 1,             // SLIDER4 (x,y)
};

void drawSliders()
{
  uint8_t slot_idx = 0;
  uint8_t max_pots = adcGetMaxInputs(ADC_INPUT_FLEX);
  uint8_t offset = adcGetInputOffset(ADC_INPUT_FLEX);

  for (uint8_t i = 0; i < max_pots; i++) {

    // TODO: move this into board implementation
#if defined(PCBX9E)
    // Only sliders
    if (!IS_SLIDER(i)) continue;
#else
    // Skip POT3
    if (i == 2 && !IS_SLIDER(i)) continue;
#endif

    coord_t x = _pot_slots[slot_idx++];
    coord_t y = _pot_slots[slot_idx++];

    lcdDrawSolidVerticalLine(x, y, LCD_H / 2 - 2);
    lcdDrawSolidVerticalLine(x + 1, y, LCD_H / 2 - 2);

    // calculate once per loop
    y += LCD_H / 2 - 4;
    y -= ((calibratedAnalogs[offset + i] + RESX) * (LCD_H / 2 - 4) / (RESX * 2));
    lcdDrawSolidVerticalLine(x - 1, y, 2);
    lcdDrawSolidVerticalLine(x + 2, y, 2);
  }
}

#define BAR_X        14
#define BAR_Y        1
#define BAR_W        184
#define BAR_H        9
#define BAR_NOTIFS_X BAR_X+133
#define BAR_VOLUME_X BAR_X+147
#define BAR_TIME_X   BAR_X+159

void displayTopBarGauge(coord_t x, int count, bool blinking=false)
{
  if (!blinking || BLINK_ON_PHASE)
    lcdDrawFilledRect(x+1, BAR_Y+2, 11, 5, SOLID, ERASE);
  for (int i=0; i<count; i+=2)
    lcdDrawSolidVerticalLine(x+2+i, BAR_Y+3, 3);
}

#define LCD_NOTIF_ICON(x, icon) \
 lcdDrawRleBitmap(x, BAR_Y, icons, icon); \
 lcdDrawSolidHorizontalLine(x, BAR_Y+8, 11)

void displayTopBar()
{
  uint8_t batt_icon_x;
  uint8_t altitude_icon_x;

  /* Tx voltage */
  putsVBat(BAR_X+2, BAR_Y+1, LEFT);
  batt_icon_x = lcdLastRightPos;
  lcdDrawRect(batt_icon_x+FW, BAR_Y+1, 13, 7);
  lcdDrawSolidVerticalLine(batt_icon_x+FW+13, BAR_Y+2, 5);

  if (TELEMETRY_STREAMING()) {
    /* RSSI */
    LCD_ICON(batt_icon_x+3*FW+3, BAR_Y, ICON_RSSI);
    lcdDrawRect(batt_icon_x+5*FW, BAR_Y+1, 13, 7);

    /* Rx voltage */
    altitude_icon_x = batt_icon_x+7*FW+3;
    if (g_model.voltsSource) {
      uint8_t item = g_model.voltsSource-1;
      if (item < MAX_TELEMETRY_SENSORS) {
        TelemetryItem & voltsItem = telemetryItems[item];
        if (voltsItem.isAvailable()) {
          drawSensorCustomValue(batt_icon_x+7*FW+2, BAR_Y+1, item, voltsItem.value, LEFT);
          altitude_icon_x = lcdLastRightPos+1;
        }
      }
    }

    /* Altitude */
    if (g_model.altitudeSource && !IS_FAI_ENABLED()) {
      uint8_t item = g_model.altitudeSource - 1;
      if (item < MAX_TELEMETRY_SENSORS) {
        TelemetryItem & altitudeItem = telemetryItems[item];
        if (altitudeItem.isAvailable()) {
          LCD_ICON(altitude_icon_x, BAR_Y, ICON_ALTITUDE);
          int32_t value = altitudeItem.value / g_model.telemetrySensors[item].getPrecDivisor();
          drawValueWithUnit(altitude_icon_x+2*FW-1, BAR_Y+1, value, g_model.telemetrySensors[item].unit, LEFT);
        }
      }
    }
  }

  /* Notifs icons */
  coord_t x = BAR_NOTIFS_X;
  if (isAsteriskDisplayed()) {
    LCD_NOTIF_ICON(x, ICON_REBOOT);
    x -= 12;
  }

  if (usbPlugged()) {
    LCD_NOTIF_ICON(x, ICON_USB);
    x -= 12;
  }

  if (logDelay100ms > 0) {
    LCD_NOTIF_ICON(x, ICON_LOGS);
    x -= 12;
  }

  /* Audio volume */
  if (requiredSpeakerVolume == 0 || g_eeGeneral.beepMode == e_mode_quiet)
    LCD_ICON(BAR_VOLUME_X, BAR_Y, ICON_SPEAKER0);
  else if (requiredSpeakerVolume <= 6)
    LCD_ICON(BAR_VOLUME_X, BAR_Y, ICON_SPEAKER1);
  else if (requiredSpeakerVolume <= 12)
    LCD_ICON(BAR_VOLUME_X, BAR_Y, ICON_SPEAKER2);
  else if (requiredSpeakerVolume <= 18)
    LCD_ICON(BAR_VOLUME_X, BAR_Y, ICON_SPEAKER2);
  else
    LCD_ICON(BAR_VOLUME_X, BAR_Y, ICON_SPEAKER3);

  /* RTC time */
  if (rtcIsValid()) drawRtcTime(BAR_TIME_X, BAR_Y+1, LEFT|TIMEBLINK);

  /* The background */
  lcdDrawFilledRect(BAR_X, BAR_Y, BAR_W, BAR_H, SOLID, FILL_WHITE|GREY(12)|ROUND);

  /* The inside of the Batt gauge */
  displayTopBarGauge(batt_icon_x+FW, GET_TXBATT_BARS(10), IS_TXBATT_WARNING());

  /* The inside of the RSSI gauge */
  if (TELEMETRY_RSSI() > 0) {
    displayTopBarGauge(batt_icon_x+5*FW, TELEMETRY_RSSI() / 10, TELEMETRY_RSSI() < g_model.rfAlarms.warning);
  }
}

void displayTimers()
{
  // Main and Second timer
  for (unsigned int i=0; i<2; i++) {
    if (timerGetState(i) != TMR_OFF) {
      const tmrval_t timerValue = timerGetValue(i);
      TimerData & timerData = g_model.timers[i];
      uint8_t y = TIMERS_Y + i*TIMERS_H;
      if (ZLEN(timerData.name) > 0) {
        lcdDrawSizedText(TIMERS_X, y-7, timerData.name, LEN_TIMER_NAME, SMLSIZE);
      }
      else {
        drawStringWithIndex(TIMERS_X, y-7, STR_TIMER, i + 1, SMLSIZE);
      }
      int val = timerValue;
      if (timerData.start && timerData.showElapsed &&
          timerData.start != timerValue)
        val = (int)timerData.start - (int)timerValue;
      drawTimer(TIMERS_X, y, val, TIMEHOUR|MIDSIZE|LEFT, TIMEHOUR|MIDSIZE|LEFT);
      if (timerData.persistent) {
        lcdDrawChar(TIMERS_R, y-7, 'P', SMLSIZE);
      }
      if (timerValue < 0) {
        if (BLINK_ON_PHASE) {
          lcdDrawFilledRect(TIMERS_X-7, y-8, 60, 20);
        }
      }
    }
  }
}

void menuMainViewChannelsMonitor(event_t event)
{
  switch(event) {
    case EVT_KEY_BREAK(KEY_PAGEDN):
    case EVT_KEY_BREAK(KEY_EXIT):
      chainMenu(menuMainView);
      event = 0;
      break;
  }

  return menuChannelsView(event);
}

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

void displaySwitch(coord_t x, coord_t y, int width, unsigned int index)
{
  if (SWITCH_EXISTS(index)) {
    int val = readPhysicalInput(physicalSwitch(index));

    if (val >= 0) {
      lcdDrawSolidHorizontalLine(x, y, width);
      lcdDrawSolidHorizontalLine(x, y+2, width);
      y += 4;
      if (val > 0) {
        lcdDrawSolidHorizontalLine(x, y, width);
        lcdDrawSolidHorizontalLine(x, y+2, width);
        y += 4;
      }
    }

    lcdDrawChar(width==5 ? x+1 : x, y, 'A'+index, TINSIZE);
    y += 6;

    if (val <= 0) {
      lcdDrawSolidHorizontalLine(x, y, width);
      lcdDrawSolidHorizontalLine(x, y+2, width);
      if (val < 0) {
        lcdDrawSolidHorizontalLine(x, y+4, width);
        lcdDrawSolidHorizontalLine(x, y+6, width);
      }
    }
  }
}

void menuMainView(event_t event)
{

  switch(event) {
    case EVT_ENTRY:
      killEvents(KEY_EXIT);
      killEvents(KEY_PLUS);
      killEvents(KEY_MINUS);
      // no break

    case EVT_ENTRY_UP:
      LOAD_MODEL_BITMAP();
      break;

    case EVT_KEY_CONTEXT_MENU:
      if (modelHasNotes()) {
        POPUP_MENU_ADD_ITEM(STR_VIEW_NOTES);
      }
      POPUP_MENU_START(onMainViewMenu, 3, STR_RESET_SUBMENU, STR_STATISTICS, STR_ABOUT_US);
      break;

    case EVT_KEY_MODEL_MENU:
      pushMenu(menuModelSelect);
      break;

    case EVT_KEY_GENERAL_MENU:
      pushMenu(menuTabGeneral[0].menuFunc);
      break;

    case EVT_KEY_NEXT_VIEW:
      storageDirty(EE_MODEL);
      g_model.view += 1;
      if (g_model.view >= VIEW_COUNT) {
        g_model.view = 0;
        chainMenu(menuMainViewChannelsMonitor);
      }
      break;

    case EVT_KEY_FIRST(KEY_EXIT):
      break;

  }

  // Model Name
  drawModelName(MODELNAME_X, MODELNAME_Y, g_model.header.name, g_eeGeneral.currModel, BIGSIZE);

  // Top bar
  displayTopBar();

  // Sliders (Pots / Sliders)
  drawSliders();

  lcdDrawBitmap(BITMAP_X, BITMAP_Y, modelBitmap);

  // Switches
  // Regular radio
  // -> 2 columns: one for each side
  // -> 8 slots on each side (2 columns of 4)

  uint8_t switches = switchGetMaxSwitches();
  if (getSwitchCount() > 16) {    // beware, there is a desired col/row swap in this special mode
    for (int i = 0; i < switches; ++i) {
      if (SWITCH_EXISTS(i) && !switchIsFlex(i)) {
        auto switch_display = switchGetDisplayPosition(i);
        if (g_model.view == VIEW_INPUTS) {
          coord_t x = 50 + (switch_display.row % 5) * 4 +
                      (switch_display.col == 0 ? 0 : 93) +
                      (switch_display.row < 5 ? 0 : 2);
          coord_t y = switch_display.row < 5 ? 25 : 40;
          displaySwitch(x, y, 3, i);
        } else {
          displaySwitch(17 + switch_display.row * 6,
                        25 + switch_display.col * 17, 5, i);
        }
      }
    }
  }
  else {
    coord_t shiftright = switchGetMaxRow(1) < 4 ? 20 : 0;
    for (int i = 0; i < switches; ++i) {
      if (SWITCH_EXISTS(i) && !switchIsFlex(i)) {
        auto switch_display = switchGetDisplayPosition(i);
        if (g_model.view == VIEW_INPUTS) {
          coord_t x = (switch_display.col == 0 ? 50 : 125) +
                      (switch_display.row < 4 ? 0 : 20) +
                      (switch_display.col == 0 ? 0 : shiftright);
          coord_t y = 25 + (switch_display.row % 4) * FH;
          getvalue_t val = readPhysicalInput(physicalSwitch(i));
          getvalue_t sw =
              ((val < 0) ? 3 * i + 1 : ((val == 0) ? 3 * i + 2 : 3 * i + 3));
          drawSwitch(x, y, sw, 0, false);
        }
        else {
          displaySwitch(17 + switch_display.row * 6,
                        25 + switch_display.col * 17, 5, i);
        }
      }
    }
  }

    if (g_model.view == VIEW_TIMERS) {
    displayTimers();
  }
  else if (g_model.view == VIEW_INPUTS) {
    // Sticks
    doMainScreenGraphics();
  }

}
