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
#include "hal/adc_driver.h"
#include "tasks/mixer_task.h"
#include <future>

class MixerTest : public EdgeTxTest {};

TEST_F(MixerTest, OutputOverridesBeforeMixerTaskInitialization)
{
  ASSERT_FALSE(mixerTaskInitialized());
  const int16_t ordinary = applyLimits(0, 0);
  // Native mutexes exist before task initialization, unlike FreeRTOS mutexes.
  // Hold this one unavailable so an accidental startup lock cannot pass silently.
  mixerTaskLock();
  auto startup = std::async(std::launch::async, [] {
    setChannelOverride(0, 75);
    const int16_t overridden = applyLimits(0, 0);
    clearChannelOverrides(); // Also called during startup model loading.
    return std::make_pair(overridden, applyLimits(0, 0));
  });
  const auto status = startup.wait_for(std::chrono::seconds(1));
  mixerTaskUnlock(); // Release even on failure, so the test cannot deadlock.
  EXPECT_EQ(std::future_status::ready, status);
  const auto values = startup.get();
  EXPECT_EQ(calc100toRESX(75), values.first);
  EXPECT_EQ(ordinary, values.second);
}

TEST_F(MixerTest, DirectOutputOverrides)
{
  clearChannelOverrides();
  const int16_t ordinary = applyLimits(0, 0);
  setChannelOverride(0, 75);
  EXPECT_EQ(calc100toRESX(75), applyLimits(0, 0));
  setChannelOverride(0, -75);
  EXPECT_EQ(calc100toRESX(-75), applyLimits(0, 0));
  setChannelOverride(0, 0, false);
  EXPECT_EQ(ordinary, applyLimits(0, 0));
  setChannelOverride(MAX_OUTPUT_CHANNELS, 100); // Invalid index is ignored.
  EXPECT_EQ(ordinary, applyLimits(0, 0));
  setChannelOverride(0, 75);
  clearChannelOverrides();
  EXPECT_EQ(ordinary, applyLimits(0, 0));
}

#define CHECK_NO_MOVEMENT(channel, value, duration) \
    for (int i=1; i<=(duration); i++) { \
      evalChannelMixes(e_perout_mode_normal, 1); \
      GTEST_ASSERT_EQ((value), chans[(channel)]); \
    }

#define CHECK_SLOW_MOVEMENT(channel, sign, duration, max_duration) \
    do { \
    for (int i=1; i<=(duration); i++) { \
      evalChannelMixes(e_perout_mode_normal, 1); \
      lastAct = lastAct + (sign) * (1<<19)/max_duration; \
      GTEST_ASSERT_EQ(256 * (lastAct >> 8), chans[(channel)]); \
    } \
    } while (0)

#define CHECK_DELAY(channel, duration) \
    do { \
      int32_t value = chans[(channel)]; \
      for (int i=1; i<=(duration); i++) { \
        evalChannelMixes(e_perout_mode_normal, 1); \
        GTEST_ASSERT_EQ(chans[(channel)], value); \
      } \
    } while (0)

  #define ELE_CHAN          1
  #define THR_STICK_MIN_THR_POS -1024

#define THR_CHAN          inputMappingGetThrottle()
#define THR_STICK         THR_CHAN
#define ELE_STICK         ELE_CHAN
#define AIL_STICK         3

#define MIXSRC_ELE        (MIXSRC_FIRST_STICK + ELE_CHAN)
#define MIXSRC_THR        (MIXSRC_FIRST_STICK + THR_CHAN)
#define MIXSRC_AIL        (MIXSRC_FIRST_STICK + AIL_STICK)

TEST_F(MixerTest, throttleInvert)
{
  // Mode 1 / reversed
  g_eeGeneral.stickMode = 0;
  g_model.throttleReversed = 1;
  anaSetFiltered(inputMappingConvertMode(THR_STICK), -1024);
  evalMixes(1);
  EXPECT_EQ(channelOutputs[THR_CHAN], +1024);

  // Mode 2 / reversed
  g_eeGeneral.stickMode = 1;
  g_model.throttleReversed = 1;
  anaSetFiltered(inputMappingConvertMode(THR_STICK), -1024);
  evalMixes(1);
  EXPECT_EQ(channelOutputs[THR_CHAN], +1024);

  // Mode 2 / normal
  g_eeGeneral.stickMode = 1;
  g_model.throttleReversed = 0;
  anaSetFiltered(inputMappingConvertMode(THR_STICK), -1024);
  evalMixes(1);
  EXPECT_EQ(channelOutputs[THR_CHAN], -1024);
}

TEST_F(MixerTest, CopySticksToOffset)
{
  anaSetFiltered(inputMappingConvertMode(ELE_STICK), -100);
  evalMixes(1);
  copySticksToOffset(ELE_CHAN);
#if defined(STICK_DEAD_ZONE)
  EXPECT_EQ(g_model.limitData[ELE_CHAN].offset, -93);
#else
  EXPECT_EQ(g_model.limitData[ELE_CHAN].offset, -97);
#endif
}

TEST(Curves, LinearIntpol)
{
  SYSTEM_RESET();
  MODEL_RESET();
  MIXER_RESET();
  setModelDefaults();
  for (int8_t i=-2; i<=2; i++) {
    g_model.points[2+i] = 50*i;
  }
  EXPECT_EQ(applyCustomCurve(-1024, 0), -1024);
  EXPECT_EQ(applyCustomCurve(0, 0), 0);
  EXPECT_EQ(applyCustomCurve(1024, 0), 1024);
  EXPECT_EQ(applyCustomCurve(-192, 0), -192);
}

TEST_F(MixerTest, InfiniteRecursiveChannels)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].srcRaw = MIXSRC_FIRST_CH + 1;
  g_model.mixData[0].weight = (100);
  g_model.mixData[1].destCh = 1;
  g_model.mixData[1].srcRaw = MIXSRC_FIRST_CH + 2;
  g_model.mixData[1].weight = (100);
  g_model.mixData[2].destCh = 2;
  g_model.mixData[2].srcRaw = MIXSRC_FIRST_CH;
  g_model.mixData[2].weight = (100);
  evalChannelMixes(e_perout_mode_normal, 0);
  EXPECT_EQ(chans[2], 0);
  EXPECT_EQ(chans[1], 0);
  EXPECT_EQ(chans[0], 0);
}

TEST_F(MixerTest, BlockingChannel)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].srcRaw = MIXSRC_FIRST_CH;
  g_model.mixData[0].weight = (100);
  evalChannelMixes(e_perout_mode_normal, 0);
  EXPECT_EQ(chans[0], 0);
}

TEST_F(MixerTest, RecursiveAddChannel)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (50);
  g_model.mixData[1].destCh = 0;
  g_model.mixData[1].mltpx = MLTPX_ADD;
  g_model.mixData[1].srcRaw = MIXSRC_FIRST_CH + 1;
  g_model.mixData[1].weight = (100);
  g_model.mixData[2].destCh = 1;
  g_model.mixData[2].srcRaw = MIXSRC_FIRST_STICK;
  g_model.mixData[2].weight = (100);

  anaSetFiltered(0, 0);
  evalChannelMixes(e_perout_mode_normal, 0);
  EXPECT_EQ(chans[0], CHANNEL_MAX/2);
  EXPECT_EQ(chans[1], 0);
}

TEST_F(MixerTest, SlowOnSwitchCondition)
{
  int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  g_model.mixData[0].swtch = SWSRC_FIRST_SWITCH + sw * 3;
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (100);
  g_model.mixData[0].speedUp = 50;
  g_model.mixData[0].speedDown = 50;

  s_mixer_first_run_done = true;
  simuSetSwitch(sw, -1);
  evalChannelMixes(e_perout_mode_normal, 0);
  EXPECT_EQ(chans[0], 0);

  CHECK_SLOW_MOVEMENT(0, +1, 250, 500);

  simuSetSwitch(sw, 1);
  CHECK_SLOW_MOVEMENT(0, -1, 250, 500);
}

TEST_F(MixerTest, SlowOnSwitchSource)
{
  int sw = findHwSwitch(SWITCH_3POS);
  if (sw < 0) return;

  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = sw + MIXSRC_FIRST_SWITCH;
  g_model.mixData[0].weight = (100);
  g_model.mixData[0].speedUp = 50;
  g_model.mixData[0].speedDown = 50;

  s_mixer_first_run_done = true;

  simuSetSwitch(sw, -1);
  CHECK_SLOW_MOVEMENT(0, -1, 250, 500);
  EXPECT_EQ(chans[0], -CHANNEL_MAX);

  simuSetSwitch(sw, 1);
  CHECK_SLOW_MOVEMENT(0, +1, 500, 500);
}

TEST_F(MixerTest, SlowOnSwitchConditionPrec10ms)
{
  int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  g_model.mixData[0].swtch = SWSRC_FIRST_SWITCH + sw * 3;
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (100);
  g_model.mixData[0].speedUp = 50;
  g_model.mixData[0].speedDown = 50;
  g_model.mixData[0].speedPrec = 1;

  s_mixer_first_run_done = true;
  simuSetSwitch(sw, -1);
  evalChannelMixes(e_perout_mode_normal, 0);
  EXPECT_EQ(chans[0], 0);

  CHECK_SLOW_MOVEMENT(0, +1, 25, 50);

  simuSetSwitch(sw, 1);
  CHECK_SLOW_MOVEMENT(0, -1, 25, 50);
}

TEST_F(MixerTest, SlowOnSwitchSourcePrec10ms)
{
  int sw = findHwSwitch(SWITCH_3POS);
  if (sw < 0) return;

  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = sw + MIXSRC_FIRST_SWITCH;
  g_model.mixData[0].weight = (100);
  g_model.mixData[0].speedUp = 50;
  g_model.mixData[0].speedDown = 50;
  g_model.mixData[0].speedPrec = 1;

  s_mixer_first_run_done = true;

  simuSetSwitch(sw, -1);
  CHECK_SLOW_MOVEMENT(0, -1, 25, 50);
  EXPECT_EQ(chans[0], -CHANNEL_MAX);

  simuSetSwitch(sw, 1);
  CHECK_SLOW_MOVEMENT(0, +1, 50, 50);
}

TEST_F(MixerTest, SlowDisabledOnStartup)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (100);
  g_model.mixData[0].speedUp = 50;
  g_model.mixData[0].speedDown = 50;

  evalChannelMixes(e_perout_mode_normal, 0);
  EXPECT_EQ(chans[0], CHANNEL_MAX);
}

TEST_F(MixerTest, DelayOnSwitch)
{
  int sw = findHwSwitch(SWITCH_3POS);
  if (sw < 0) return;
  int swPos = (sw * 3) + SWSRC_FIRST_SWITCH + 2;

  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (100);
  g_model.mixData[0].swtch = swPos;
  g_model.mixData[0].delayUp = 50;
  g_model.mixData[0].delayDown = 50;

  simuSetSwitch(sw, -1);

  evalChannelMixes(e_perout_mode_normal, 0);
  EXPECT_EQ(chans[0], 0);

  simuSetSwitch(sw, 1);
  CHECK_DELAY(0, 500);

  evalChannelMixes(e_perout_mode_normal, 1);
  EXPECT_EQ(chans[0], CHANNEL_MAX);

  simuSetSwitch(sw, 0);
  CHECK_DELAY(0, 500);

  evalChannelMixes(e_perout_mode_normal, 1);
  EXPECT_EQ(chans[0], 0);
}

TEST_F(MixerTest, DelayOnSwitch2)
{
  int sw = findHwSwitch(SWITCH_3POS);
  if (sw < 0) return;

  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = sw + MIXSRC_FIRST_SWITCH;
  g_model.mixData[0].weight = (100);
  g_model.mixData[0].delayUp = 50;
  g_model.mixData[0].delayDown = 50;

  simuSetSwitch(sw, -1);

  evalChannelMixes(e_perout_mode_normal, 0);
  EXPECT_EQ(chans[0], 0);

  simuSetSwitch(sw, 1);
  CHECK_DELAY(0, 500);

  evalChannelMixes(e_perout_mode_normal, 1);
  EXPECT_EQ(chans[0], CHANNEL_MAX);

  simuSetSwitch(sw, 0);
  CHECK_DELAY(0, 500);

  evalChannelMixes(e_perout_mode_normal, 1);
  EXPECT_EQ(chans[0], 0);
}

TEST_F(MixerTest, SlowOnMultiply)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (100);
  g_model.mixData[1].destCh = 0;
  g_model.mixData[1].mltpx = MLTPX_MUL;
  g_model.mixData[1].srcRaw = MIXSRC_MAX;
  g_model.mixData[1].weight = (100);
  g_model.mixData[1].swtch = SWSRC_FIRST_SWITCH;
  g_model.mixData[1].speedUp = 50;
  g_model.mixData[1].speedDown = 50;

  s_mixer_first_run_done = true;

  simuSetSwitch(0, 1);
  CHECK_SLOW_MOVEMENT(0, 1, 250, 500);

  simuSetSwitch(0, -1);
  CHECK_NO_MOVEMENT(0, CHANNEL_MAX, 250);

  simuSetSwitch(0, 1);
  CHECK_NO_MOVEMENT(0, CHANNEL_MAX, 250);
}

TEST_F(MixerTest, SlowOnMultiplyPrec10ms)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (100);
  g_model.mixData[1].destCh = 0;
  g_model.mixData[1].mltpx = MLTPX_MUL;
  g_model.mixData[1].srcRaw = MIXSRC_MAX;
  g_model.mixData[1].weight = (100);
  g_model.mixData[1].swtch = SWSRC_FIRST_SWITCH;
  g_model.mixData[1].speedUp = 50;
  g_model.mixData[1].speedDown = 50;
  g_model.mixData[1].speedPrec = 1;

  s_mixer_first_run_done = true;

  simuSetSwitch(0, 1);
  CHECK_SLOW_MOVEMENT(0, 1, 25, 50);

  simuSetSwitch(0, -1);
  CHECK_NO_MOVEMENT(0, CHANNEL_MAX, 250);

  simuSetSwitch(0, 1);
  CHECK_NO_MOVEMENT(0, CHANNEL_MAX, 250);
}

// ==========================================================================
// rc-soar.com documented behavior tests
// https://rc-soar.com/edgetx/index.php
// Enshrine mixer and channel-cascade semantics so that
// refactoring does not silently break real-world user setups.
// ==========================================================================

// Multiplex ADD: lines accumulate additively on the same channel.
TEST_F(MixerTest, MultiplexAdd)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (60);
  g_model.mixData[1].destCh = 0;
  g_model.mixData[1].mltpx = MLTPX_ADD;
  g_model.mixData[1].srcRaw = MIXSRC_MAX;
  g_model.mixData[1].weight = (40);

  evalChannelMixes(e_perout_mode_normal, 0);
  EXPECT_EQ(chans[0], CHANNEL_MAX);  // 60% + 40% = 100%
}

// Multiplex REPL: second line replaces whatever the first produced.
TEST_F(MixerTest, MultiplexReplace)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (100);
  g_model.mixData[1].destCh = 0;
  g_model.mixData[1].mltpx = MLTPX_REPL;
  g_model.mixData[1].srcRaw = MIXSRC_MAX;
  g_model.mixData[1].weight = (-50);

  evalChannelMixes(e_perout_mode_normal, 0);
  EXPECT_EQ(chans[0], -CHANNEL_MAX / 2);  // REPL overwrites to -50%
}

// Multiplex MUL: multiplies with the result of all lines above.
TEST_F(MixerTest, MultiplexMultiplyBasic)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (100);
  g_model.mixData[1].destCh = 0;
  g_model.mixData[1].mltpx = MLTPX_MUL;
  g_model.mixData[1].srcRaw = MIXSRC_MAX;
  g_model.mixData[1].weight = (50);

  evalChannelMixes(e_perout_mode_normal, 0);
  EXPECT_EQ(chans[0], CHANNEL_MAX / 2);  // 100% * 50% = 50%
}

// Multiplex MUL is order-sensitive: ADD+ADD+MUL differs from ADD+MUL+ADD.
TEST_F(MixerTest, MultiplexMultiplyOrderSensitive)
{
  // Case A: ADD(60) + ADD(40) + MUL(50) = (60+40)*50% = 50%
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (60);
  g_model.mixData[1].destCh = 0;
  g_model.mixData[1].mltpx = MLTPX_ADD;
  g_model.mixData[1].srcRaw = MIXSRC_MAX;
  g_model.mixData[1].weight = (40);
  g_model.mixData[2].destCh = 0;
  g_model.mixData[2].mltpx = MLTPX_MUL;
  g_model.mixData[2].srcRaw = MIXSRC_MAX;
  g_model.mixData[2].weight = (50);

  evalChannelMixes(e_perout_mode_normal, 0);
  int32_t caseA = chans[0];
  EXPECT_EQ(caseA, CHANNEL_MAX / 2);  // 100% * 50% = 50%

  // Case B: ADD(60) + MUL(50) + ADD(40) = 60*50% + 40 = 30+40 = 70%
  MIXER_RESET();
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (60);
  g_model.mixData[1].destCh = 0;
  g_model.mixData[1].mltpx = MLTPX_MUL;
  g_model.mixData[1].srcRaw = MIXSRC_MAX;
  g_model.mixData[1].weight = (50);
  g_model.mixData[2].destCh = 0;
  g_model.mixData[2].mltpx = MLTPX_ADD;
  g_model.mixData[2].srcRaw = MIXSRC_MAX;
  g_model.mixData[2].weight = (40);

  evalChannelMixes(e_perout_mode_normal, 0);
  int32_t caseB = chans[0];

  EXPECT_NE(caseA, caseB);  // order matters
}

// Weight-then-offset: output = (source * weight) + offset.
TEST_F(MixerTest, WeightThenOffset)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].srcRaw = MIXSRC_FIRST_STICK;
  g_model.mixData[0].weight = (50);
  g_model.mixData[0].offset = (50);

  // Stick at +100%: 50% of 1024 + 50% offset
  anaSetFiltered(inputMappingConvertMode(0), +1024);
  evalChannelMixes(e_perout_mode_normal, 0);
  int32_t full = chans[0];

  // Stick at 0: 0 + 50% offset
  anaSetFiltered(inputMappingConvertMode(0), 0);
  evalChannelMixes(e_perout_mode_normal, 0);
  int32_t mid = chans[0];

  // Stick at -100%: -50% + 50% offset = 0
  anaSetFiltered(inputMappingConvertMode(0), -1024);
  evalChannelMixes(e_perout_mode_normal, 0);
  int32_t low = chans[0];

  // full should be ~100%, mid ~50%, low ~0%
  EXPECT_NEAR(full, CHANNEL_MAX, CHANNEL_MAX / 100);
  EXPECT_NEAR(mid, CHANNEL_MAX / 2, CHANNEL_MAX / 100);
  EXPECT_NEAR(low, 0, CHANNEL_MAX / 100);
}

// Cascaded channels bypass output clipping: a channel used as a mix source
// carries its internal (>100%) value, not the clipped output.
TEST_F(MixerTest, CascadedChannelBypassesOutputClipping)
{
  // CH0: stick at 200% weight (overdrives to 200% internally)
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].srcRaw = MIXSRC_FIRST_STICK;
  g_model.mixData[0].weight = (200);
  // CH1: reads CH0 at 50% weight — if unclipped, 200%*50% = 100%
  g_model.mixData[1].destCh = 1;
  g_model.mixData[1].srcRaw = MIXSRC_FIRST_CH;
  g_model.mixData[1].weight = (50);

  anaSetFiltered(inputMappingConvertMode(0), +1024);
  evalMixes(1);

  // CH0 output is clipped to 100%
  EXPECT_EQ(channelOutputs[0], 1024);
  // CH1 sees the unclipped CH0 (200%), applies 50% → 100%
  // If CH0 were clipped before cascading, CH1 would be only 50%.
  EXPECT_GT(channelOutputs[1], 512);  // must be more than 50%
  EXPECT_NEAR(channelOutputs[1], 1024, 2);  // should be ~100%
}

// Cumulative weight through cascaded channels:
// CH0 = stick * 80%, CH1 = CH0 * 25% → CH1 should be stick * 20%.
TEST_F(MixerTest, CascadedWeightMultiplication)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].srcRaw = MIXSRC_FIRST_STICK;
  g_model.mixData[0].weight = (80);
  g_model.mixData[1].destCh = 1;
  g_model.mixData[1].srcRaw = MIXSRC_FIRST_CH;
  g_model.mixData[1].weight = (25);

  anaSetFiltered(inputMappingConvertMode(0), +1024);
  evalMixes(1);

  // CH0 = 80% of 1024 ≈ 819
  EXPECT_NEAR(channelOutputs[0], 1024 * 80 / 100, 2);
  // CH1 = 25% of CH0 = 20% of 1024 ≈ 205
  EXPECT_NEAR(channelOutputs[1], 1024 * 20 / 100, 2);
}

// Sequencer pattern (rc-soar.com/edgetx/setups/sequencer):
// CH0 = timebase driven by a switch with slow up/down (linear ramp).
// CH1 = servo channel reading CH0 through a custom curve.
// A flat curve segment produces a "pause" where the servo holds position
// while the timebase continues to ramp.
TEST_F(MixerTest, SequencerSlowRampThroughCurve)
{
  int sw = findHwSwitch(SWITCH_3POS);
  if (sw < 0) return;

  // --- Curve setup: 5-point standard curve on slot 0 ---
  // Points at x = -100, -50, 0, +50, +100 (standard = evenly spaced)
  // y-values: -100, -100, 0, 0, +100
  //   Segment 1 (-100..-50): flat at -100 (pause)
  //   Segment 2 (-50..0):    ramp from -100 to 0
  //   Segment 3 (0..+50):    flat at 0 (pause)
  //   Segment 4 (+50..+100): ramp from 0 to +100
  g_model.curves[0].type = CURVE_TYPE_STANDARD;
  g_model.curves[0].smooth = 0;  // linear interpolation, no smoothing
  g_model.curves[0].points = 0;  // 0 means 5 points (default)
  int8_t *pts = curveAddress(0);
  pts[0] = -100;  // x=-100%
  pts[1] = -100;  // x=-50%  (flat: pause)
  pts[2] =    0;  // x=0%
  pts[3] =    0;  // x=+50%  (flat: pause)
  pts[4] =  100;  // x=+100%

  // --- CH0: timebase — switch source with slow ---
  // speedUp=10 → 1.0s full traversal (-100% to +100%), 100 ticks
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].mltpx = MLTPX_ADD;
  g_model.mixData[0].srcRaw = sw + MIXSRC_FIRST_SWITCH;
  g_model.mixData[0].weight = (100);
  g_model.mixData[0].speedUp = 10;    // 1.0s
  g_model.mixData[0].speedDown = 10;  // 1.0s

  // --- CH1: servo — reads CH0 through curve ---
  g_model.mixData[1].destCh = 1;
  g_model.mixData[1].mltpx = MLTPX_ADD;
  g_model.mixData[1].srcRaw = MIXSRC_FIRST_CH;
  g_model.mixData[1].weight = (100);
  g_model.mixData[1].curve.type = CURVE_REF_CUSTOM;
  g_model.mixData[1].curve.value = (1);  // curve index 0 (1-based)

  s_mixer_first_run_done = true;

  // Start: switch up → CH0 targets -100%
  simuSetSwitch(sw, -1);
  for (int i = 0; i < 110; i++)
    evalChannelMixes(e_perout_mode_normal, 1);
  evalMixes(1);  // run limits to populate channelOutputs

  int16_t timebaseStart = channelOutputs[0];
  int16_t servoStart = channelOutputs[1];
  // Timebase at -100%, curve maps -100→-100
  EXPECT_NEAR(timebaseStart, -1024, 20);
  EXPECT_NEAR(servoStart, -1024, 20);

  // Flip switch down → ramp begins from -100% toward +100%
  simuSetSwitch(sw, 1);

  // Advance 25 ticks (25% of 1.0s = timebase at ~-50%)
  // Curve: flat from -100% to -50%, so servo should still be near -100%
  for (int i = 0; i < 25; i++)
    evalChannelMixes(e_perout_mode_normal, 1);
  evalMixes(1);

  int16_t timebaseQ1 = channelOutputs[0];
  int16_t servoQ1 = channelOutputs[1];
  EXPECT_GT(timebaseQ1, timebaseStart + 100) << "Timebase should have moved";
  EXPECT_NEAR(servoQ1, -1024, 60) << "Servo should hold during flat segment (pause)";

  // Advance to 50 ticks total (50% = timebase at ~0%)
  // Curve: ramp from -100 to 0, so servo should be near 0%
  for (int i = 0; i < 25; i++)
    evalChannelMixes(e_perout_mode_normal, 1);
  evalMixes(1);

  int16_t timebaseMid = channelOutputs[0];
  int16_t servoMid = channelOutputs[1];
  EXPECT_NEAR(timebaseMid, 0, 60);
  EXPECT_NEAR(servoMid, 0, 60);

  // Advance to 75 ticks (75% = timebase at ~+50%)
  // Curve: flat from 0 to +50%, so servo should hold near 0%
  for (int i = 0; i < 25; i++)
    evalChannelMixes(e_perout_mode_normal, 1);
  evalMixes(1);

  int16_t timebaseQ3 = channelOutputs[0];
  int16_t servoQ3 = channelOutputs[1];
  EXPECT_GT(timebaseQ3, timebaseMid + 100) << "Timebase should have advanced";
  // Servo should be near the flat segment value (0), but may slightly
  // overshoot into the final ramp segment due to discrete stepping.
  EXPECT_NEAR(servoQ3, 0, 150) << "Servo should be near flat segment";
  EXPECT_LT(servoQ3, 512) << "Servo must not yet reach the final ramp";

  // Advance to ~100 ticks (100% = timebase at +100%)
  // Curve: ramp from 0 to +100, so servo should be near +100%
  for (int i = 0; i < 30; i++)
    evalChannelMixes(e_perout_mode_normal, 1);
  evalMixes(1);

  int16_t timebaseEnd = channelOutputs[0];
  int16_t servoEnd = channelOutputs[1];
  EXPECT_NEAR(timebaseEnd, 1024, 20);
  EXPECT_NEAR(servoEnd, 1024, 40);

  // Reverse: flip switch up — sequence should reverse automatically
  simuSetSwitch(sw, -1);
  for (int i = 0; i < 110; i++)
    evalChannelMixes(e_perout_mode_normal, 1);
  evalMixes(1);

  EXPECT_NEAR(channelOutputs[0], -1024, 20) << "Timebase should return to -100%";
  EXPECT_NEAR(channelOutputs[1], -1024, 40) << "Servo should return to start via reversed curve";
}

TEST_F(MixerTest, PhysicalTrimButtonsDoNotOffsetSticks)
{
  // Even nonzero bytes left at old trim storage positions must have no effect.
  g_model.reservedThrTrim = 1;
  g_model.reservedExtendedTrims = 1;
  g_model.reservedTrimInc = 2;

  for (int mode = 0; mode < 4; ++mode) {
    g_eeGeneral.stickMode = mode;
    for (int axis = 0; axis < 4; ++axis)
      anaSetFiltered(axis, -600 + axis * 350);
    evalChannelMixes(e_perout_mode_normal, 1);
    int32_t baseline[MAX_OUTPUT_CHANNELS];
    memcpy(baseline, chans, sizeof(baseline));
    for (int button = 0; button < keysGetMaxTrims() * 2; ++button) {
      simuSetTrim(button, true);
      for (int tick = 0; tick < 30; ++tick) {
        evalChannelMixes(e_perout_mode_normal, 1);
        for (int ch = 0; ch < MAX_OUTPUT_CHANNELS; ++ch)
          ASSERT_EQ(baseline[ch], chans[ch]) << "button=" << button;
      }
      simuSetTrim(button, false);
    }

  }
}

TEST_F(MixerTest, PhysicalTrimDirectionsAreIndependentMomentarySwitches)
{
  for (int mode = 0; mode < 4; ++mode) {
    g_eeGeneral.stickMode = mode;
    for (int button = 0; button < keysGetMaxTrims() * 2; ++button) {
      // A button can gate an ordinary mix without any trim mode setup.
      auto& mix = g_model.mixData[4];
      mix.destCh = 4;
      mix.srcRaw = MIXSRC_MAX;
      mix.weight = 100;
      mix.swtch = SWSRC_FIRST_TRIM + button;
      simuSetTrim(button, true);
      for (int other = 0; other < keysGetMaxTrims() * 2; ++other)
        EXPECT_EQ(other == button, getSwitch(SWSRC_FIRST_TRIM + other));
      evalMixes(1);
      EXPECT_EQ(RESX, channelOutputs[4]);
      simuSetTrim(button, false);
      EXPECT_FALSE(getSwitch(SWSRC_FIRST_TRIM + button));
      evalMixes(1);
      EXPECT_EQ(0, channelOutputs[4]);
    }
  }
}

TEST_F(MixerTest, RemovedTrimValueSourcesAreUnavailable)
{
  for (int source = MIXSRC_FIRST_RESERVED_TRIM; source <= MIXSRC_LAST_RESERVED_TRIM; ++source) {
    EXPECT_FALSE(isSourceAvailable(source));
    bool valid = true;
    EXPECT_EQ(0, getValue(source, &valid));
    EXPECT_FALSE(valid);
  }
}

// checkIncDecMovedSwitch belongs to monochrome menu navigation.
#if defined(AUTOSWITCH) && !defined(COLORLCD)
TEST_F(MixerTest, PhysicalTrimButtonsAutoSelectWithoutTrimModes)
{
  getMovedSwitch(); // Establish the polling baseline.
  for (int button = 0; button < keysGetMaxTrims() * 2; ++button) {
    ++g_tmr10ms;
    simuSetTrim(button, true);
    EXPECT_EQ(SWSRC_FIRST_TRIM + button, checkIncDecMovedSwitch(SWSRC_NONE));
    ++g_tmr10ms;
    simuSetTrim(button, false);
    getMovedSwitch();
  }
}
#endif

TEST_F(MixerTest, NegativeLiteralInputAndMixValues)
{
  g_model.expoData[0].srcRaw = MIXSRC_MAX;
  g_model.expoData[0].mode = 3;
  g_model.expoData[0].weight = -50;
  g_model.expoData[0].offset = -25;
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].srcRaw = MIXSRC_FIRST_INPUT;
  g_model.mixData[0].weight = -100;
  g_model.mixData[0].offset = -25;
  evalMixes(1);
  EXPECT_NEAR(anas[0], -3 * RESX / 4, 2);
  EXPECT_NEAR(channelOutputs[0], RESX / 2, 2);
}
