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
