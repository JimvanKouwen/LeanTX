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

#include "dataconstants.h"
#include "gtests.h"
#include "myeeprom.h"

#include "hal/adc_driver.h"
#include "hal/switch_driver.h"

TEST(getSwitch, nullSW)
{
  EXPECT_TRUE(getSwitch(SWSRC_NONE));
}

uint8_t boardGetMaxSwitches();

TEST(FlexSwitches, switchGetPosition)
{
  if (adcGetMaxInputs(ADC_INPUT_FLEX) == 0) return;
  if (MAX_FLEX_SWITCHES == 0) return;
  switchInit();

  auto sw_idx = boardGetMaxSwitches();
  auto sw_name = switchGetDefaultName(sw_idx);
  EXPECT_STREQ("FL1", sw_name);
  EXPECT_FALSE(switchIsFlexValid(sw_idx));

  // Configure 1st FLEX input as switch
  g_eeGeneral.potsConfig = FLEX_SWITCH;
  switchConfigFlex(sw_idx, 0);
  EXPECT_TRUE(switchIsFlexValid(sw_idx));

  auto offset = adcGetInputOffset(ADC_INPUT_FLEX);
  anaSetFiltered(offset, -1024);
  EXPECT_EQ(SWITCH_HW_UP, switchGetPosition(sw_idx));

  anaSetFiltered(offset, 0);
  EXPECT_EQ(SWITCH_HW_MID, switchGetPosition(sw_idx));

  anaSetFiltered(offset, +1024);
  EXPECT_EQ(SWITCH_HW_DOWN, switchGetPosition(sw_idx));
}

TEST(FlexSwitches, getValue)
{
  if (adcGetMaxInputs(ADC_INPUT_FLEX) == 0) return;
  if (MAX_FLEX_SWITCHES == 0) return;
  switchInit();

  // Configure 1st FLEX input as switch
  g_eeGeneral.potsConfig = FLEX_SWITCH;
  auto sw_idx = boardGetMaxSwitches();
  switchConfigFlex(sw_idx, 0);

  g_eeGeneral.switchSetType(sw_idx, SWITCH_3POS);
  EXPECT_EQ(SWITCH_3POS, g_model.getSwitchType(sw_idx));

  auto offset = adcGetInputOffset(ADC_INPUT_FLEX);
  anaSetFiltered(offset, -1024);
  EXPECT_EQ(-1024, getValue(MIXSRC_FIRST_SWITCH + sw_idx));

  anaSetFiltered(offset, 0);
  EXPECT_EQ(0, getValue(MIXSRC_FIRST_SWITCH + sw_idx));

  anaSetFiltered(offset, +1024);
  EXPECT_EQ(+1024, getValue(MIXSRC_FIRST_SWITCH + sw_idx));

  g_eeGeneral.switchSetType(sw_idx, SWITCH_2POS);
  EXPECT_EQ(SWITCH_2POS, g_model.getSwitchType(sw_idx));

  anaSetFiltered(offset, -1024);
  EXPECT_EQ(-1024, getValue(MIXSRC_FIRST_SWITCH + sw_idx));

  anaSetFiltered(offset, 0);
  EXPECT_EQ(+1024, getValue(MIXSRC_FIRST_SWITCH + sw_idx));

  anaSetFiltered(offset, +1024);
  EXPECT_EQ(+1024, getValue(MIXSRC_FIRST_SWITCH + sw_idx));
}

TEST(FlexSwitches, getSwitch)
{
  if (adcGetMaxInputs(ADC_INPUT_FLEX) == 0) return;
  if (MAX_FLEX_SWITCHES == 0) return;
  switchInit();

  // Configure 1st FLEX input as switch
  g_eeGeneral.potsConfig = FLEX_SWITCH;
  auto sw_idx = boardGetMaxSwitches();
  switchConfigFlex(sw_idx, 0);

  g_eeGeneral.switchSetType(sw_idx, SWITCH_3POS);
  EXPECT_EQ(SWITCH_3POS, g_model.getSwitchType(sw_idx));

  auto offset = adcGetInputOffset(ADC_INPUT_FLEX);
  anaSetFiltered(offset, -1024);
  EXPECT_TRUE(getSwitch(SWSRC_FIRST_SWITCH + sw_idx * 3));
  EXPECT_FALSE(getSwitch(SWSRC_FIRST_SWITCH + sw_idx * 3 + 1));
  EXPECT_FALSE(getSwitch(SWSRC_FIRST_SWITCH + sw_idx * 3 + 2));

  anaSetFiltered(offset, 0);
  EXPECT_FALSE(getSwitch(SWSRC_FIRST_SWITCH + sw_idx * 3));
  EXPECT_TRUE(getSwitch(SWSRC_FIRST_SWITCH + sw_idx * 3 + 1));
  EXPECT_FALSE(getSwitch(SWSRC_FIRST_SWITCH + sw_idx * 3 + 2));

  anaSetFiltered(offset, +1024);
  EXPECT_FALSE(getSwitch(SWSRC_FIRST_SWITCH + sw_idx * 3));
  EXPECT_FALSE(getSwitch(SWSRC_FIRST_SWITCH + sw_idx * 3 + 1));
  EXPECT_TRUE(getSwitch(SWSRC_FIRST_SWITCH + sw_idx * 3 + 2));
}

#if defined(FUNCTION_SWITCHES)
TEST(FunctionSwitches, holdIsNotRepeatedToggle)
{
  MODEL_RESET();
  setModelDefaults();
  switchInit();

  for (uint8_t i = 0; i < switchGetMaxSwitches(); i++) {
    if (!switchIsCustomSwitch(i)) continue;
    // Toggle type with no group, so a phantom "switch moved" event
    // flips the logical state and is observable
    g_model.cfsSetType(i, SWITCH_TOGGLE);
    g_model.cfsSetGroup(i, 0);
    simuSetSwitch(i, -1);
  }

  setFSStartupPosition();
  evalFunctionSwitches();  // sync previous state with all switches released

  for (uint8_t i = 0; i < switchGetMaxSwitches(); i++) {
    if (!switchIsCustomSwitch(i)) continue;

    bool released = g_model.cfsState(i);
    simuSetSwitch(i, 1);
    evalFunctionSwitches();
    bool pressed = g_model.cfsState(i);
    EXPECT_NE(released, pressed) << "press did not toggle switch index " << (int)i;

    // holding the switch must not generate further toggles
    evalFunctionSwitches();
    EXPECT_EQ(pressed, g_model.cfsState(i))
        << "hold re-toggled switch index " << (int)i;
    evalFunctionSwitches();
    EXPECT_EQ(pressed, g_model.cfsState(i))
        << "hold re-toggled switch index " << (int)i;

    simuSetSwitch(i, -1);
    evalFunctionSwitches();
  }
}
#endif

TEST(getSwitch, PhysicalPositionsAndInversion)
{
  MODEL_RESET();
  RADIO_RESET();
  const int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  const int first = SWSRC_FIRST_SWITCH + 3 * sw;
  for (int pos = 0; pos < 3; ++pos) {
    simuSetSwitch(sw, pos - 1);
    for (int selected = 0; selected < 3; ++selected) {
      EXPECT_EQ(pos == selected, getSwitch(first + selected));
      EXPECT_EQ(pos != selected, getSwitch(-(first + selected)));
      EXPECT_TRUE(isSwitchAvailableInMixes(first + selected));
      EXPECT_TRUE(isSwitchAvailableInMixes(-(first + selected)));
    }
  }
  EXPECT_FALSE(getSwitch(SWSRC_COUNT));
  EXPECT_FALSE(getSwitch(-SWSRC_COUNT));
  EXPECT_FALSE(isSwitchAvailableInMixes(SWSRC_COUNT));
}
