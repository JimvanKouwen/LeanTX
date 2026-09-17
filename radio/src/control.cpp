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
#include "timers.h"
#if defined(RADIO_GX12)
#include "targets/taranis/gx12/bsp_io.h"
#endif

uint8_t controlsInitialized = false;
int16_t channelOutputs[MAX_OUTPUT_CHANNELS] = {};

void updateChannelOutputs()
{
#if defined(RADIO_GX12)
  _poll_switches();
#endif
  evalAnalogControls();
  for (unsigned channel = 0; channel < MAX_OUTPUT_CHANNELS; ++channel)
    channelOutputs[channel] = readPhysicalInput(g_model.channelMappings[channel].source);
}

void doMixerPeriodicUpdates()
{
  static tmr10ms_t lastTMR = 0;

  tmr10ms_t tmr10ms = get_tmr10ms();

  uint8_t tick10ms = (tmr10ms >= lastTMR ? tmr10ms - lastTMR : 1);
  // handle tick10ms overrun
  // correct overflow handling costs a lot of code; happens only each 11 min;
  // therefore forget the exact calculation and use only 1 instead; good compromise
  lastTMR = tmr10ms;

  DEBUG_TIMER_START(debugTimerMixes10ms);
  if (tick10ms) {
    evalTimers(tick10ms);

    static uint8_t  s_cnt_100ms;
    static uint8_t  s_cnt_1s;

    if ((s_cnt_100ms += tick10ms) >= 10) { // 0.1sec
      s_cnt_100ms -= 10;
      s_cnt_1s += 1;

      if (s_cnt_1s >= 10) { // 1sec
        s_cnt_1s -= 10;
        sessionTimer += 1;
        inactivity.counter++;
        if ((((uint8_t)inactivity.counter) & 0x07) == 0x01 && g_eeGeneral.inactivityTimer && inactivity.counter > ((uint16_t)g_eeGeneral.inactivityTimer * 60))
          AUDIO_INACTIVITY();
      }
    }
  }

  DEBUG_TIMER_STOP(debugTimerMixes10ms);

  controlsInitialized = true;
}
