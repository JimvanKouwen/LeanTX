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

#include "gtests.h"

namespace {
void initTimer(uint8_t idx, int start = 0)
{
  g_model.timers[idx] = TimerData{};
  g_model.timers[idx].start = start;
  timerReset(idx);
}
void advance(unsigned seconds)
{
  while (seconds--) evalTimers(100);
}
}

TEST(Timers, ExplicitStartStopResume)
{
  initTimer(0);
  advance(10);
  EXPECT_EQ(0, timersStates[0].val);
  timerStart(0);
  evalTimers(75);
  timerStop(0);
  advance(10);
  EXPECT_EQ(0, timersStates[0].val);
  timerStart(0);
  evalTimers(25);
  EXPECT_EQ(1, timersStates[0].val);
  timerStart(0); // Starting an active timer is idempotent.
  advance(24 * 3600);
  EXPECT_EQ(86401, timersStates[0].val);
}

TEST(Timers, ResetAndSetStop)
{
  initTimer(0, 200);
  timerStart(0);
  advance(1);
  timerReset(0);
  advance(10);
  EXPECT_EQ(200, timersStates[0].val);
  EXPECT_EQ(TMR_OFF, timersStates[0].state);
  EXPECT_FALSE(timersStates[0].running);
  timerStart(0);
  timerSet(0, 500);
  advance(10);
  EXPECT_EQ(500, timersStates[0].val);
  EXPECT_FALSE(timersStates[0].running);
}

TEST(Timers, CountdownStates)
{
  initTimer(0, 2);
  timerStart(0);
  advance(1);
  EXPECT_EQ(1, timersStates[0].val);
  EXPECT_EQ(TMR_RUNNING, timersStates[0].state);
  advance(1);
  EXPECT_EQ(0, timersStates[0].val);
  EXPECT_EQ(TMR_NEGATIVE, timersStates[0].state);
  timerStop(0);
  advance(100);
  EXPECT_EQ(0, timersStates[0].val);
  timerStart(0);
  advance(60);
  EXPECT_EQ(-60, timersStates[0].val);
  EXPECT_EQ(TMR_STOPPED, timersStates[0].state);
  advance(1); // Alert period ended, counting continues.
  EXPECT_EQ(-61, timersStates[0].val);
}

TEST(Timers, BatchedTicksAndIndependentLimits)
{
  initTimer(0);
  initTimer(1, 1);
  initTimer(2);
  timerSet(0, TIMER_MAX - 1);
  timerSet(1, TIMER_MIN + 1);
  for (int i = 0; i < TIMERS; ++i) timerStart(i);
  evalTimers(250);
  evalTimers(250);
  EXPECT_EQ(TIMER_MAX, timersStates[0].val);
  EXPECT_EQ(TIMER_MIN, timersStates[1].val);
  EXPECT_EQ(5, timersStates[2].val);
  EXPECT_EQ(0, timersStates[2].val_10ms);
}

TEST(Timers, SaveRestoreDoesNotStart)
{
  initTimer(0, 200);
  initTimer(1);
  g_model.timers[0].persistent = 1;
  g_model.timers[1].persistent = 1;
  timerSet(0, -25);
  timerSet(1, 100000);
  saveTimers();
  EXPECT_EQ(-25, g_model.timers[0].value);
  EXPECT_EQ(100000, g_model.timers[1].value);
  timerReset(0);
  timerReset(1);
  restoreTimers();
  advance(10);
  EXPECT_EQ(-25, timersStates[0].val);
  EXPECT_EQ(100000, timersStates[1].val);
  EXPECT_FALSE(timersStates[0].running);
  timerStart(0);
  advance(1);
  EXPECT_EQ(-26, timersStates[0].val);
  EXPECT_EQ(TMR_NEGATIVE, timersStates[0].state);
}
