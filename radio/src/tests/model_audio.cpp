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

#include "model_audio.h"

#include "gtests.h"

TEST(ModelAudio, switches)
{
  int sw_pos;
  EXPECT_TRUE(matchSwitchAudioFile("sa-up.wav", sw_pos));
  EXPECT_EQ(0, sw_pos);

  EXPECT_TRUE(matchSwitchAudioFile("Sa-mid.wav", sw_pos));
  EXPECT_EQ(1, sw_pos);

  EXPECT_TRUE(matchSwitchAudioFile("SA-down.wav", sw_pos));
  EXPECT_EQ(2, sw_pos);

  EXPECT_FALSE(matchSwitchAudioFile("SA-dow.wav", sw_pos));
  EXPECT_FALSE(matchSwitchAudioFile("SX-mid.wav", sw_pos));
  EXPECT_FALSE(matchSwitchAudioFile("AS-mid.wav", sw_pos));

  g_eeGeneral.potsConfig = FLEX_MULTIPOS << (POT_CFG_BITS * 2);
  EXPECT_TRUE(matchSwitchAudioFile("S34.wav", sw_pos));
  EXPECT_EQ(MAX_SWITCHES * 3 + 2 * XPOTS_MULTIPOS_COUNT + 3, sw_pos);

  sw_pos = 123;
  EXPECT_FALSE(matchSwitchAudioFile("S12.wav", sw_pos));
  EXPECT_EQ(123, sw_pos);
}

class ModelAudioTest : public EdgeTxTest {};

TEST_F(ModelAudioTest, HardwarePositionFilenamesRoundTrip)
{
  char path[AUDIO_FILENAME_MAXLEN + 1];
  const char* suffixes[] = {"-up.wav", "-mid.wav", "-down.wav"};
  for (int sw = 0; sw < switchGetMaxAllSwitches(); ++sw) {
    for (int pos = 0; pos < 3; ++pos) {
      ASSERT_TRUE(getSwitchAudioFile(path, sw * 3 + pos));
      const char* filename = strrchr(path, '/') + 1;
      std::string expected = std::string(switchGetDefaultName(sw)) + suffixes[pos];
      EXPECT_STREQ(expected.c_str(), filename);
      int slot = -1;
      ASSERT_TRUE(matchSwitchAudioFile(filename, slot));
      EXPECT_EQ(sw * 3 + pos, slot);
    }
  }
  for (int pot = 0; pot < MAX_POTS; ++pot) {
    g_eeGeneral.potsConfig = FLEX_MULTIPOS << (POT_CFG_BITS * pot);
    for (int pos = 0; pos < XPOTS_MULTIPOS_COUNT; ++pos) {
      int slot = MAX_SWITCHES * 3 + pot * XPOTS_MULTIPOS_COUNT + pos;
      ASSERT_TRUE(getSwitchAudioFile(path, slot));
      const char* filename = strrchr(path, '/') + 1;
      char expected[] = "S11.wav";
      expected[1] += pot;
      expected[2] += pos;
      EXPECT_STREQ(expected, filename);
      int matched = -1;
      ASSERT_TRUE(matchSwitchAudioFile(filename, matched));
      EXPECT_EQ(slot, matched);
    }
  }
  EXPECT_FALSE(getSwitchAudioFile(path, MAX_SWITCHES * 3 + MAX_POTS * XPOTS_MULTIPOS_COUNT));
}

TEST_F(ModelAudioTest, HardwareTransitionsKeepMiddlePositionDelay)
{
  extern uint64_t switchesPos;
  int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  g_eeGeneral.switchesDelay = 10;
  g_tmr10ms = 100;
  simuSetSwitch(sw, -1);
  getSwitchesPosition(true);
  const uint64_t up = uint64_t(1) << (sw * 3);
  const uint64_t mid = up << 1;
  const uint64_t down = up << 2;
  const uint64_t mask = up | mid | down;
  EXPECT_EQ(up, switchesPos & mask);
  simuSetSwitch(sw, 0);
  getSwitchesPosition(false);
  EXPECT_EQ(up, switchesPos & mask);
  g_tmr10ms += 100;
  getSwitchesPosition(false);
  EXPECT_EQ(mid, switchesPos & mask);
  simuSetSwitch(sw, 1);
  getSwitchesPosition(false);
  EXPECT_EQ(down, switchesPos & mask);
}
