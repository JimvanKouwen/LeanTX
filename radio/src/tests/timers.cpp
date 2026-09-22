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
  EXPECT_EQ(0, timerGetValue(0));
  timerStart(0);
  evalTimers(75);
  timerStop(0);
  advance(10);
  EXPECT_EQ(0, timerGetValue(0));
  timerStart(0);
  evalTimers(25);
  EXPECT_EQ(1, timerGetValue(0));
  timerStart(0); // Starting an active timer is idempotent.
  advance(24 * 3600);
  EXPECT_EQ(86401, timerGetValue(0));
}

TEST(Timers, ResetAndSetStop)
{
  initTimer(0, 200);
  timerStart(0);
  advance(1);
  timerReset(0);
  advance(10);
  EXPECT_EQ(200, timerGetValue(0));
  EXPECT_EQ(TMR_OFF, timerGetState(0));
  EXPECT_FALSE(timerIsRunning(0));
  timerStart(0);
  timerSet(0, 500);
  advance(10);
  EXPECT_EQ(500, timerGetValue(0));
  EXPECT_FALSE(timerIsRunning(0));
}

TEST(Timers, CountdownStates)
{
  initTimer(0, 2);
  timerStart(0);
  advance(1);
  EXPECT_EQ(1, timerGetValue(0));
  EXPECT_EQ(TMR_RUNNING, timerGetState(0));
  advance(1);
  EXPECT_EQ(0, timerGetValue(0));
  EXPECT_EQ(TMR_NEGATIVE, timerGetState(0));
  timerStop(0);
  advance(100);
  EXPECT_EQ(0, timerGetValue(0));
  timerStart(0);
  advance(60);
  EXPECT_EQ(-60, timerGetValue(0));
  EXPECT_EQ(TMR_STOPPED, timerGetState(0));
  advance(1); // Alert period ended, counting continues.
  EXPECT_EQ(-61, timerGetValue(0));
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
  EXPECT_EQ(TIMER_MAX, timerGetValue(0));
  EXPECT_EQ(TIMER_MIN, timerGetValue(1));
  EXPECT_EQ(5, timerGetValue(2));
  evalTimers(99);
  EXPECT_EQ(5, timerGetValue(2));
  evalTimers(1);
  EXPECT_EQ(6, timerGetValue(2));
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
  EXPECT_EQ(-25, timerGetValue(0));
  EXPECT_EQ(100000, timerGetValue(1));
  EXPECT_FALSE(timerIsRunning(0));
  timerStart(0);
  advance(1);
  EXPECT_EQ(-26, timerGetValue(0));
  EXPECT_EQ(TMR_NEGATIVE, timerGetState(0));
}

TEST(Timers, ValueOnlyUpdatePreservesRuntimeState)
{
  initTimer(0, 10);
  timerStart(0);
  evalTimers(75);
  timerSetValue(0, 5);
  EXPECT_EQ(5, timerGetValue(0));
  EXPECT_TRUE(timerIsRunning(0));
  EXPECT_EQ(TMR_RUNNING, timerGetState(0));
  evalTimers(25);
  EXPECT_EQ(4, timerGetValue(0));
  advance(4);
  timerStop(0);
  timerSetValue(0, -20);
  EXPECT_FALSE(timerIsRunning(0));
  EXPECT_EQ(TMR_NEGATIVE, timerGetState(0));
  advance(1);
  EXPECT_EQ(-20, timerGetValue(0));
}

TEST(Timers, ResetAndSetClearFractionalTime)
{
  initTimer(0);
  timerStart(0);
  evalTimers(75);
  timerReset(0);
  timerStart(0);
  evalTimers(25);
  EXPECT_EQ(0, timerGetValue(0));
  timerSet(0, 100);
  EXPECT_EQ(TMR_OFF, timerGetState(0));
  EXPECT_FALSE(timerIsRunning(0));
  timerStart(0);
  evalTimers(75);
  EXPECT_EQ(100, timerGetValue(0));
  evalTimers(25);
  EXPECT_EQ(101, timerGetValue(0));
}

TEST(Timers, InvalidAccessDoesNotChangeTimers)
{
  initTimer(0);
  timerSetValue(0, 123);
  for (int idx : {-1, TIMERS}) {
    timerSetValue(idx, 456);
    timerSet(idx, 456);
    timerStart(idx);
    timerStop(idx);
    timerReset(idx);
    EXPECT_EQ(0, timerGetValue(idx));
    EXPECT_FALSE(timerIsRunning(idx));
    EXPECT_EQ(TMR_OFF, timerGetState(idx));
  }
  EXPECT_EQ(123, timerGetValue(0));
}
