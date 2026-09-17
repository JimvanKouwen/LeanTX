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

#pragma once

#include "edgetx_types.h"

#define TMR_OFF      0
#define TMR_RUNNING  1
#define TMR_NEGATIVE 2
#define TMR_STOPPED  3 // Post-expiry alerts finished; the timer can still run.

typedef int32_t tmrval_t;
typedef uint32_t tmrstart_t;
#define TIMER_MAX     (0xffffff/2)

#define TIMER_MIN     (tmrval_t(-TIMER_MAX-1))

struct TimerState {
  bool running; // Explicit start/stop, independent of the alert state.
  uint8_t  state;
  tmrval_t  val;
  uint8_t  val_10ms;
};

#if defined(TIMERS)
extern TimerState timersStates[TIMERS];
#endif

// Start/resume and pause without changing the value or fractional second.
void timerStart(uint8_t idx);
void timerStop(uint8_t idx);
// Reset/set clear fractional time and leave the timer paused.
void timerReset(uint8_t idx);

void timerSet(int idx, int val);

void saveTimers();
void restoreTimers();

void evalTimers(uint8_t tick10ms);
