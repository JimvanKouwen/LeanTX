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

void playValue(mixsrc_t idx, uint8_t id, int8_t fragmentVolume)
{
  if (IS_FAI_FORBIDDEN(idx))
    return;

  if (idx == MIXSRC_NONE)
    return;

  getvalue_t val = getValue(idx);
  idx = abs(idx); // Don't need negative form any longer

  if (idx >= MIXSRC_FIRST_TELEM) {
    TelemetrySensor & telemetrySensor = g_model.telemetrySensors[(idx-MIXSRC_FIRST_TELEM) / 3];
    uint8_t attr = 0;

    // Preserve the sign
    int sign = (val >= 0) ? 1 : -1;
    val = abs(val);

    if (telemetrySensor.prec > 0) {
      if (telemetrySensor.prec == 2) {
        if (val >= 5000) {
          val = divRoundClosest(val, 100);
        }
        else {
          val = divRoundClosest(val, 10);
          attr = PREC1;
        }
      }
      else {
        if (val >= 500) {
          val = divRoundClosest(val, 10);
        }
        else {
          attr = PREC1;
        }
      }
    }

    val *= sign; // Reapply sign if needed

    PLAY_NUMBER(val, telemetrySensor.unit == UNIT_CELLS ? UNIT_VOLTS : telemetrySensor.unit, attr);
  }
  else if (idx >= MIXSRC_FIRST_TIMER && idx <= MIXSRC_LAST_TIMER) {
    int flag = 0;
    if (abs(val) > LONG_TIMER_DURATION) {
      flag = PLAY_LONG_TIMER;
    }
    PLAY_DURATION(val, flag);
  } else if (idx == MIXSRC_TX_TIME) {
    PLAY_DURATION(val * 60, PLAY_TIME);
  } else if (idx == MIXSRC_TX_VOLTAGE) {
    PLAY_NUMBER(val, UNIT_VOLTS, PREC1);
#if defined(LUMINOSITY_SENSOR)
  } else if (idx == MIXSRC_LIGHT) {
    PLAY_NUMBER(val, UNIT_RAW, 0);
#endif
  } else {
    if (idx <= MIXSRC_LAST_CH) {
      val = calcRESXto100(val);
    }
    PLAY_NUMBER(val, 0, 0);
  }
}
