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
#include "timers.h"

volatile tmr10ms_t g_tmr10ms;

#define MAX_ALERT_TIME   60

#if TIMERS > MAX_TIMERS
#error "Timers cannot exceed " .. MAX_TIMERS
#endif

TimerState timersStates[TIMERS] = { { 0 } };

void timerStart(uint8_t idx)
{
  if (idx >= TIMERS) return;
  auto &timer = timersStates[idx];
  timer.running = true;
  if (timer.state == TMR_OFF)
    timer.state = g_model.timers[idx].start && timer.val <= 0
                      ? (timer.val <= -MAX_ALERT_TIME ? TMR_STOPPED : TMR_NEGATIVE)
                      : TMR_RUNNING;
}

void timerStop(uint8_t idx)
{
  if (idx < TIMERS) timersStates[idx].running = false;
}

void timerReset(uint8_t idx)
{
  if (idx >= TIMERS) return;
  TimerState & timerState = timersStates[idx];
  timerState.running = false;
  timerState.state = TMR_OFF;
  timerState.val = g_model.timers[idx].start;
  timerState.val_10ms = 0 ;
}

void timerSet(int idx, int val)
{
  if (idx < 0 || idx >= TIMERS) return;
  TimerState & timerState = timersStates[idx];
  timerState.running = false;
  timerState.state = TMR_OFF;
  timerState.val = val;
  timerState.val_10ms = 0 ;
}

void restoreTimers()
{
  for (uint8_t i=0; i<TIMERS; i++) {
    if (g_model.timers[i].persistent) {
      timerSet(i, g_model.timers[i].value);
    }
  }
}

void saveTimers()
{
  for (uint8_t i=0; i<TIMERS; i++) {
    if (g_model.timers[i].persistent) {
      TimerState *timerState = &timersStates[i];
      if (g_model.timers[i].value != timerState->val) {
        g_model.timers[i].value = timerState->val;
        storageDirty(EE_MODEL);
      }
    }
  }
}

void evalTimers(uint8_t tick10ms)
{
  for (uint8_t i = 0; i < TIMERS; i++) {
    TimerState *timerState = &timersStates[i];
    if (!timerState->running) continue;
    const tmrstart_t timerStart = g_model.timers[i].start;
    const bool showElapsed = g_model.timers[i].showElapsed;
    uint16_t elapsed = timerState->val_10ms + tick10ms;
    timerState->val_10ms = elapsed % 100;
    while (elapsed >= 100) {
      elapsed -= 100;
      if ((!timerStart && timerState->val >= TIMER_MAX) ||
          (timerStart && timerState->val <= TIMER_MIN)) break;
      tmrval_t newTimerVal = timerStart ? timerStart - timerState->val : timerState->val;
      newTimerVal++;

      switch (timerState->state) {
        case TMR_RUNNING:
          if (timerStart && newTimerVal >= (tmrval_t)timerStart) {
            AUDIO_TIMER_ELAPSED(i);
            timerState->state = TMR_NEGATIVE;
            // TRACE("Timer[%d] negative", i);
          }
          break;
        case TMR_NEGATIVE:
          if (newTimerVal >= (tmrval_t)timerStart + MAX_ALERT_TIME) {
            timerState->state = TMR_STOPPED;
            // TRACE("Timer[%d] stopped state at %d", i, newTimerVal);
          }
          break;
      }

      // if counting backwards - display backwards
      if (timerStart) newTimerVal = timerStart - newTimerVal;

      if (newTimerVal != timerState->val) {
        timerState->val = newTimerVal;
        if (timerState->state == TMR_RUNNING) {
          if (g_model.timers[i].countdownBeep && g_model.timers[i].start) {
            AUDIO_TIMER_COUNTDOWN(i, newTimerVal);
          }
          tmrval_t announceVal = newTimerVal;
          if (showElapsed) announceVal = timerStart - newTimerVal;
          if (g_model.timers[i].minuteBeep && (announceVal % 60) == 0) {
            AUDIO_TIMER_MINUTE(announceVal);
            // TRACE("Timer[%d] %d minute announcement", i, newTimerVal/60);
          }
        }
      }
    }
  }
}
