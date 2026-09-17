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

class MixerTest : public EdgeTxTest {};

TEST_F(MixerTest, PhysicalMappingPassesThroughEveryChannel)
{
  MODEL_RESET();
#if defined(STICK_DEAD_ZONE)
  g_eeGeneral.stickDeadZone = 0;
#endif
  for (int ch = 0; ch < MAX_OUTPUT_CHANNELS; ++ch) {
    g_model.mixData[ch].srcRaw = MIXSRC_FIRST_STICK;
    g_model.mixData[ch].destCh = ch;
    g_model.mixData[ch].weight = 100;
  }
  for (int value : {-RESX, -777, -1, 0, 1, 777, RESX}) {
    anaSetFiltered(inputMappingConvertMode(0), value);
    evalMixes();
    for (int ch = 0; ch < MAX_OUTPUT_CHANNELS; ++ch) {
      EXPECT_EQ(value, channelOutputs[ch]) << ch;
      EXPECT_EQ(value, ex_chans[ch]) << ch;
    }
  }
  // Existing mapping arithmetic may exceed nominal endpoints. It must reach
  // the protocol encoder unchanged in either direction.
  for (int ch = 0; ch < MAX_OUTPUT_CHANNELS; ++ch)
    g_model.mixData[ch].weight = 200;
  for (int value : {-RESX, RESX}) {
    anaSetFiltered(inputMappingConvertMode(0), value);
    evalMixes();
    for (int ch = 0; ch < MAX_OUTPUT_CHANNELS; ++ch)
      EXPECT_EQ(2 * value, channelOutputs[ch]) << ch;
  }
  memclear(g_model.mixData, sizeof(g_model.mixData));
  evalMixes();
  for (int ch = 0; ch < MAX_OUTPUT_CHANNELS; ++ch)
    EXPECT_EQ(0, channelOutputs[ch]) << ch;
}

// Telemetry IDs remain readable by Lua/UI, including min/max and inversion,
// but cannot activate ordinary control lines loaded from older models or Lua.
TEST_F(MixerTest, DirectTelemetrySourcesAreIgnored)
{
  MODEL_RESET();
  MIXER_RESET();
  telemetryItems[0].value = 321;
  telemetryItems[0].valueMin = 123;
  telemetryItems[0].valueMax = 456;
  const int values[] = {321, 123, 456};
  EXPECT_LT(MIXSRC_LAST, MIXSRC_FIRST_TELEM);
  for (int variant = 0; variant < 3; ++variant) {
    for (int sign : {-1, 1}) {
      const auto source = sign * (MIXSRC_FIRST_TELEM + variant);
      EXPECT_EQ(sign * values[variant], getValue(source));
      auto &mix = g_model.mixData[0];
      mix.srcRaw = source;
      mix.weight = 100;
      mix.offset = 50;
      for (auto mode : {e_perout_mode_normal, e_perout_mode_preview}) {
        evalChannelMixes(mode);
        EXPECT_EQ(0, chans[0]);
      }
    }
  }
  telemetryItems[0].clear();
}

// A non-control line must be skipped before offset processing, even
// when its shared value is zero or the source was injected by Lua/storage.
TEST_F(MixerTest, NonControlSourceFamiliesCannotRoute)
{
  MODEL_RESET();
  MIXER_RESET();
  const int rejected[] = {
    MIXSRC_TX_VOLTAGE, MIXSRC_TX_TIME, MIXSRC_TX_GPS,
    MIXSRC_FIRST_TIMER, MIXSRC_LAST_TIMER,
    MIXSRC_FIRST_TELEM, MIXSRC_LAST_TELEM,
    MIXSRC_FIRST_SWITCH - 1, MIXSRC_INVERT,
#if defined(IMU)
    MIXSRC_TILT_X, MIXSRC_TILT_Y,
#endif
#if defined(LUMINOSITY_SENSOR)
    MIXSRC_LIGHT,
#endif
#if defined(PCBHORUS)
    MIXSRC_FIRST_SPACEMOUSE, MIXSRC_LAST_SPACEMOUSE,
#endif
#if defined(FUNCTION_SWITCHES)
    MIXSRC_FIRST_CUSTOMSWITCH_GROUP, MIXSRC_LAST_CUSTOMSWITCH_GROUP,
#endif
  };
  for (int source : rejected) {
    for (int sign : {-1, 1}) {
      EXPECT_FALSE(isMixerSourceAvailable(sign * source)) << source;
      auto &mix = g_model.mixData[0];
      mix.srcRaw = sign * source;
      mix.weight = 100;
      mix.offset = 50;
      for (auto mode : {e_perout_mode_normal, e_perout_mode_preview}) {
        evalChannelMixes(mode);
        EXPECT_EQ(0, chans[0]) << source;
      }
    }
  }
  EXPECT_FALSE(isMixerSourceAvailable(MIXSRC_NONE));
}

TEST_F(MixerTest, SharedSystemAndTimerValuesRemainReadable)
{
  g_vbat100mV = 83;
  timersStates[0].val = 123;
  EXPECT_EQ(83, getValue(MIXSRC_TX_VOLTAGE));
  EXPECT_EQ(-83, getValue(-MIXSRC_TX_VOLTAGE));
  EXPECT_EQ(123, getValue(MIXSRC_FIRST_TIMER));
  EXPECT_EQ(-123, getValue(-MIXSRC_FIRST_TIMER));
  EXPECT_FALSE(isMixerSourceAvailable(MIXSRC_TX_VOLTAGE));
  EXPECT_FALSE(isMixerSourceAvailable(MIXSRC_FIRST_TIMER));
}

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
  evalMixes();
  EXPECT_EQ(channelOutputs[THR_CHAN], +1024);

  // Mode 2 / reversed
  g_eeGeneral.stickMode = 1;
  g_model.throttleReversed = 1;
  anaSetFiltered(inputMappingConvertMode(THR_STICK), -1024);
  evalMixes();
  EXPECT_EQ(channelOutputs[THR_CHAN], +1024);

  // Mode 2 / normal
  g_eeGeneral.stickMode = 1;
  g_model.throttleReversed = 0;
  anaSetFiltered(inputMappingConvertMode(THR_STICK), -1024);
  evalMixes();
  EXPECT_EQ(channelOutputs[THR_CHAN], -1024);
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
  evalChannelMixes(e_perout_mode_normal);
  EXPECT_EQ(chans[2], 0);
  EXPECT_EQ(chans[1], 0);
  EXPECT_EQ(chans[0], 0);
}

TEST_F(MixerTest, BlockingChannel)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].srcRaw = MIXSRC_FIRST_CH;
  g_model.mixData[0].weight = (100);
  evalChannelMixes(e_perout_mode_normal);
  EXPECT_EQ(chans[0], 0);
}

TEST_F(MixerTest, RecursiveAddChannel)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (50);
  g_model.mixData[1].destCh = 0;
  g_model.mixData[1].srcRaw = MIXSRC_FIRST_CH + 1;
  g_model.mixData[1].weight = (100);
  g_model.mixData[2].destCh = 1;
  g_model.mixData[2].srcRaw = MIXSRC_FIRST_STICK;
  g_model.mixData[2].weight = (100);

  anaSetFiltered(0, 0);
  evalChannelMixes(e_perout_mode_normal);
  EXPECT_EQ(chans[0], CHANNEL_MAX/2);
  EXPECT_EQ(chans[1], 0);
}

TEST_F(MixerTest, SwitchSourceAndCascadeApplyImmediately)
{
  int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  g_model.mixData[0].srcRaw = MIXSRC_FIRST_SWITCH + sw;
  g_model.mixData[0].weight = 100;
  auto& cascade = g_model.mixData[1];
  cascade.destCh = 1;
  cascade.srcRaw = MIXSRC_FIRST_CH;
  cascade.weight = 100;

  s_mixer_first_run_done = true;
  for (int position : {-1, 1, 0, -1}) {
    simuSetSwitch(sw, position);
    evalMixes();
    EXPECT_EQ(chans[0], position * CHANNEL_MAX);
    EXPECT_EQ(chans[1], position * CHANNEL_MAX);
  }
}

// ==========================================================================
// rc-soar.com documented behavior tests
// https://rc-soar.com/edgetx/index.php
// Enshrine mixer and channel-cascade semantics so that
// refactoring does not silently break real-world user setups.
// ==========================================================================

TEST_F(MixerTest, MappingsSum)
{
  g_model.mixData[0].destCh = 0;
  g_model.mixData[0].srcRaw = MIXSRC_MAX;
  g_model.mixData[0].weight = (60);
  g_model.mixData[1].destCh = 0;
  g_model.mixData[1].srcRaw = MIXSRC_MAX;
  g_model.mixData[1].weight = (40);

  evalChannelMixes(e_perout_mode_normal);
  EXPECT_EQ(chans[0], CHANNEL_MAX);  // 60% + 40% = 100%
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
  evalChannelMixes(e_perout_mode_normal);
  int32_t full = chans[0];

  // Stick at 0: 0 + 50% offset
  anaSetFiltered(inputMappingConvertMode(0), 0);
  evalChannelMixes(e_perout_mode_normal);
  int32_t mid = chans[0];

  // Stick at -100%: -50% + 50% offset = 0
  anaSetFiltered(inputMappingConvertMode(0), -1024);
  evalChannelMixes(e_perout_mode_normal);
  int32_t low = chans[0];

  // full should be ~100%, mid ~50%, low ~0%
  EXPECT_NEAR(full, CHANNEL_MAX, CHANNEL_MAX / 100);
  EXPECT_NEAR(mid, CHANNEL_MAX / 2, CHANNEL_MAX / 100);
  EXPECT_NEAR(low, 0, CHANNEL_MAX / 100);
}

// Cascaded channels and transmitted channels retain the same mapped value.
TEST_F(MixerTest, CascadedChannelsRetainMappedValues)
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
  evalMixes();

  // CH0 retains 200% without an Outputs-stage endpoint clamp.
  EXPECT_EQ(channelOutputs[0], 2048);
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
  evalMixes();

  // CH0 = 80% of 1024 ≈ 819
  EXPECT_NEAR(channelOutputs[0], 1024 * 80 / 100, 2);
  // CH1 = 25% of CH0 = 20% of 1024 ≈ 205
  EXPECT_NEAR(channelOutputs[1], 1024 * 20 / 100, 2);
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
    evalChannelMixes(e_perout_mode_normal);
    int32_t baseline[MAX_OUTPUT_CHANNELS];
    memcpy(baseline, chans, sizeof(baseline));
    for (int button = 0; button < keysGetMaxTrims() * 2; ++button) {
      simuSetTrim(button, true);
      for (int tick = 0; tick < 30; ++tick) {
        evalChannelMixes(e_perout_mode_normal);
        for (int ch = 0; ch < MAX_OUTPUT_CHANNELS; ++ch)
          ASSERT_EQ(baseline[ch], chans[ch]) << "button=" << button;
      }
      simuSetTrim(button, false);
    }

  }
}

TEST_F(MixerTest, RemovedTrimValueSourcesAreUnavailable)
{
  for (int source = MIXSRC_FIRST_SWITCH - MAX_TRIMS; source < MIXSRC_FIRST_SWITCH; ++source) {
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

TEST_F(MixerTest, MappingSumIsOrderIndependentAndPassesThrough)
{
  MODEL_RESET();
  for (int i = 0; i < 3; ++i) {
    g_model.mixData[i].srcRaw = MIXSRC_MAX;
    g_model.mixData[i].weight = i == 0 ? -50 : 100;
  }
  for (int i = 0; i < 3; ++i) {
    evalMixes();
    EXPECT_EQ(CHANNEL_MAX * 3 / 2, chans[0]);
    EXPECT_EQ(RESX * 3 / 2, channelOutputs[0]);
    std::swap(g_model.mixData[0], g_model.mixData[i]);
  }
}

TEST_F(MixerTest, InvertedChannelSourcesChainInBothDirections)
{
  MODEL_RESET();
  for (bool forward : {false, true}) {
    g_model.mixData[0].destCh = 0;
    g_model.mixData[1].destCh = 1;
    auto& source = g_model.mixData[forward ? 1 : 0];
    auto& dest = g_model.mixData[forward ? 0 : 1];
    source.srcRaw = MIXSRC_MAX;
    source.weight = 50;
    dest.srcRaw = -(MIXSRC_FIRST_CH + source.destCh);
    dest.weight = 100;
    evalMixes();
    EXPECT_EQ(CHANNEL_MAX / 2, chans[source.destCh]);
    EXPECT_EQ(-CHANNEL_MAX / 2, chans[dest.destCh]);
  }
}

TEST_F(MixerTest, DefaultQuadUsesPhysicalAETRControls)
{
  generalDefault();
  setModelDefaults();
  const int sticks[] = {3, 1, 2, 0}; // Ail, Ele, Thr, Rud
#if defined(STICK_DEAD_ZONE)
  g_eeGeneral.stickDeadZone = 0;
#endif
  for (int mode = 0; mode < 4; ++mode) {
    g_eeGeneral.stickMode = mode;
    for (int ch = 0; ch < 4; ++ch) {
      EXPECT_EQ(MIXSRC_FIRST_STICK + sticks[ch], g_model.mixData[ch].srcRaw);
      EXPECT_EQ(ch, g_model.mixData[ch].destCh);
      anaSetFiltered(inputMappingConvertMode(sticks[ch]), (ch - 2) * 256);
    }
    evalMixes();
    for (int ch = 0; ch < 4; ++ch)
      EXPECT_EQ((ch - 2) * 256, channelOutputs[ch]);
  }
}

TEST_F(MixerTest, PhysicalSourceWeightOffsetAndRemoval)
{
  MODEL_RESET();
#if defined(STICK_DEAD_ZONE)
  g_eeGeneral.stickDeadZone = 0;
#endif
  auto& mix = g_model.mixData[0];
  mix.srcRaw = MIXSRC_FIRST_STICK;
  mix.weight = -50;
  mix.offset = 25;
  for (int value : {-RESX, -RESX / 2, 0, RESX / 2, RESX}) {
    anaSetFiltered(inputMappingConvertMode(0), value);
    evalMixes();
    EXPECT_NEAR(-value / 2 + RESX / 4, channelOutputs[0], 1);
  }
  mix = MixData{};
  evalMixes();
  EXPECT_EQ(0, channelOutputs[0]);
}
