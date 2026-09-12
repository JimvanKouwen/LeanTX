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
#include "options.h"
#include "hal/module_port.h"

#if defined(CROSSFIRE)
  #include "mixer_scheduler.h"
#endif

#include "fw_version.h"

#define MENU_BODY_TOP    (FH + 1)
#define MENU_BODY_BOTTOM (LCD_H)

#if defined(CROSSFIRE)
constexpr uint8_t COLUMN2_X = 10 * FW;
#endif

void menuRadioFirmwareOptions(event_t event)
{
  title(STR_MENU_FIRM_OPTIONS);

  coord_t y = MENU_HEADER_HEIGHT + 1;
  lcdNextPos = INDENT_WIDTH;

  for (uint8_t i=0; options[i]; i++) {
    const char * option = options[i];

    if (i > 0) {
      lcdDrawText(lcdNextPos, y, ", ");
    }

    if (lcdNextPos + getTextWidth(option) + 5 > LCD_W) {
      lcdNextPos = INDENT_WIDTH;
      y += FH;
    }

    lcdDrawText(lcdNextPos, y, option);
  }

  if (event == EVT_KEY_BREAK(KEY_EXIT)) {
    popMenu();
  }
}

#if defined(CROSSFIRE)
void menuRadioModulesVersion(event_t event)
{
  if (menuEvent) {
    for (uint8_t i = 0; i < MAX_MODULES; i++) {
      moduleState[i].mode = MODULE_MODE_NORMAL;
    }
    return;
  }

  title(STR_MENU_MODULES_RX_VERSION);

  coord_t y = (FH + 1) - menuVerticalOffset * FH;

  for (uint8_t module=0; module<NUM_MODULES; module++) {
    // Label
    if (y >= MENU_BODY_TOP && y < MENU_BODY_BOTTOM) {
#if defined(HARDWARE_INTERNAL_MODULE)
      if (module == INTERNAL_MODULE)
        lcdDrawTextAlignedLeft(y, STR_INTERNAL_MODULE);
#endif
#if defined(HARDWARE_EXTERNAL_MODULE)
      if (module == EXTERNAL_MODULE)
        lcdDrawTextAlignedLeft(y, STR_EXTERNAL_MODULE);
#endif
    }
    y += FH;

    // Module model
    if (y >= MENU_BODY_TOP && y < MENU_BODY_BOTTOM) {
      lcdDrawTextIndented(y, STR_MODULE);
      bool module_off = true;
#if defined(HARDWARE_INTERNAL_MODULE)
      if (module == INTERNAL_MODULE && modulePortPowered(INTERNAL_MODULE))
        module_off = false;
#endif
#if defined(HARDWARE_EXTERNAL_MODULE)
      if (module == EXTERNAL_MODULE && modulePortPowered(EXTERNAL_MODULE))
        module_off = false;
#endif
      if (module_off) {
        lcdDrawText(COLUMN2_X, y, STR_OFF);
        y += FH;
        continue;
      }
#if defined(CROSSFIRE)
      if (isModuleCrossfire(module)) {
        char statusText[64] = "";
        sprintf(statusText, "%d Hz"/* %" PRIu32" Err"*/,
                1000000 / getMixerSchedulerPeriod()/*, UINT32_C(telemetryErrors)*/);
        lcdDrawText(COLUMN2_X, y, statusText);
        y += FH;
        lcdDrawText(INDENT_WIDTH, y, crossfireModuleStatus[module].name);
        lcdDrawChar(lcdNextPos + 5, y, 'V');
        lcdDrawNumber(lcdNextPos, y, crossfireModuleStatus[module].major);
        lcdDrawChar(lcdNextPos, y, '.');
        lcdDrawNumber(lcdNextPos, y, crossfireModuleStatus[module].minor);
        lcdDrawChar(lcdNextPos, y, '.');
        lcdDrawNumber(lcdNextPos, y, crossfireModuleStatus[module].revision);
        y += FH;
        continue;
      }
#endif
      {
        lcdDrawText(COLUMN2_X, y, STR_NO_INFORMATION);
        y += FH;
        continue;
      }
    }
    y += FH;
  }

  uint8_t lines = (y - (FH + 1)) / FH + menuVerticalOffset;
  if (lines > NUM_BODY_LINES) {
    drawVerticalScrollbar(LCD_W-1, FH, LCD_H-FH, menuVerticalOffset, lines, NUM_BODY_LINES);
  }

  if (IS_PREVIOUS_EVENT(event)) {
    if (lines > NUM_BODY_LINES) {
      if (menuVerticalOffset-- == 0)
        menuVerticalOffset = lines - 1;
    }
  } else if (IS_NEXT_EVENT(event)) {
    if (lines > NUM_BODY_LINES) {
      if (++menuVerticalOffset + NUM_BODY_LINES > lines)
        menuVerticalOffset = 0;
    }
  } else if (event == EVT_KEY_BREAK(KEY_EXIT)) {
    if (menuVerticalOffset != 0)
      menuVerticalOffset = 0;
    else
      popMenu();
  }
}
#endif

enum MenuRadioVersionItems
{
  ITEM_RADIO_VERSION_FIRST = HEADER_LINE - 1,
#if defined(PCBTARANIS)
  ITEM_RADIO_FIRMWARE_OPTIONS,
#endif
#if defined(CROSSFIRE)
  ITEM_RADIO_MODULES_VERSION,
#endif
  ITEM_RADIO_VERSION_COUNT
};

void menuRadioVersion(event_t event)
{
  SIMPLE_MENU(STR_MENUVERSION, menuTabGeneral, MENU_RADIO_VERSION, ITEM_RADIO_VERSION_COUNT);

  coord_t y = MENU_HEADER_HEIGHT + 2;
  lcdDrawText(FW, y, vers_stamp, SMLSIZE);
  y += 5 * (FH - 1) + 2;

#if defined(PCBTARANIS)
  lcdDrawText(INDENT_WIDTH, y, STR_FIRMWARE_OPTIONS, menuVerticalPosition == ITEM_RADIO_FIRMWARE_OPTIONS ? INVERS : 0);
  y += FH;
  if (menuVerticalPosition == ITEM_RADIO_FIRMWARE_OPTIONS && event == EVT_KEY_BREAK(KEY_ENTER)) {
    s_editMode = EDIT_SELECT_FIELD;
    pushMenu(menuRadioFirmwareOptions);
  }
#endif

#if defined(CROSSFIRE)
  lcdDrawText(INDENT_WIDTH, y, STR_MODULES_RX_VERSION, menuVerticalPosition == ITEM_RADIO_MODULES_VERSION ? INVERS : 0);
  y += FH;
  if (menuVerticalPosition == ITEM_RADIO_MODULES_VERSION && event == EVT_KEY_BREAK(KEY_ENTER)) {
    s_editMode = EDIT_SELECT_FIELD;
    pushMenu(menuRadioModulesVersion);
  }
#endif
}
