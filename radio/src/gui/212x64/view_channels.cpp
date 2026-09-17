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

struct ViewChannelsState {
    bool secondPage;
};

static ViewChannelsState viewChannels = { false };

void menuChannelsView(event_t event)
{
  uint8_t ch = 0;
  uint8_t wbar = 64;
  int16_t limits = 512 * 2;

  if (g_eeGeneral.ppmunit == PPM_PERCENT_PREC1) {
    wbar -= 6;
  }

  switch(event) {
    case EVT_KEY_BREAK(KEY_EXIT):
      popMenu();
      break;

    case EVT_ROTARY_LEFT:
    case EVT_ROTARY_RIGHT:
    case EVT_KEY_FIRST(KEY_PLUS):
    case EVT_KEY_FIRST(KEY_MINUS):
      viewChannels.secondPage = !viewChannels.secondPage;
      break;

  }

  if (viewChannels.secondPage)
    ch = 16;


  lcdDrawText(LCD_W / 2, 0, STR_CHANNELS_MONITOR, CENTERED);
  lcdInvertLine(0);

  // Column separator
  lcdDrawSolidVerticalLine(LCD_W/2, FH, LCD_H-FH);

  for (uint8_t col=0; col < 2; col++) {
    const uint8_t x = col * LCD_W / 2 + 1;
    const uint8_t ofs = (col ? 0 : 1);

    // Channels
    for (uint8_t line=0; line < 8; line++) {
      const uint8_t y = 9 + line * 7;
      const int32_t val = channelOutputs[ch];
      putsChn(x+1-ofs, y, ch+1, SMLSIZE);

      // Value
      if (g_eeGeneral.ppmunit == PPM_US) {
        lcdDrawNumber(x+LCD_W/2-3-wbar-ofs, y+1, PPM_CENTER+val/2, TINSIZE|RIGHT);
      } else if (g_eeGeneral.ppmunit == PPM_PERCENT_PREC1) {
        lcdDrawNumber(x+LCD_W/2-3-wbar-ofs, y+1, calcRESXto1000(val), PREC1|TINSIZE|RIGHT);
      } else {
        lcdDrawNumber(x+LCD_W/2-3-wbar-ofs, y+1, calcRESXto1000(val)/10, TINSIZE|RIGHT);
      }

      // Gauge
      drawGauge(x+LCD_W/2-3-wbar-ofs, y, wbar, 6, val, limits);

      ++ch;
    }
  }
}
