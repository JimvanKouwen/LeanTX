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

#include "storage/radio_config_adapter.h"

static const char _radio_config[] =
    "potsConfig: \n"
    "   P1:\n"
    "      type: without_detent\n"
    "      name: \"\"\n"
    "   P2:\n"
    "      type: multipos_switch\n"
    "      name: \"\"\n"
    "   P3:\n"
    "      type: with_detent\n"
    "      name: \"\"\n"
    "   SL1:\n"
    "      type: slider\n"
    "      name: \"\"\n"
    "   SL2:\n"
    "      type: slider\n"
    "      name: \"\"\n"
    "switchConfig: \n"
    "   SA:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SB:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SC:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SD:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SE:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SF:\n"
    "      type: 2pos\n"
    "      name: \"\"\n"
    "   SG:\n"
    "      type: 3pos\n"
    "      name: \"\"\n"
    "   SH:\n"
    "      type: toggle\n"
    "      name: \"\"\n";

static void loadRadioYamlStr(const char* str)
{
  const char* cursor = str;
  config_stream::Workspace workspace;
  auto result = config_stream::process({&cursor, [](void* context) {
    auto& p = *static_cast<const char**>(context);
    return *p ? int((unsigned char)*p++) : -1;
  }, nullptr}, {}, radioSettingsSchema(), workspace, true);
  ASSERT_TRUE(result) << (result.error ? result.error : "");

}

TEST(Sources, getSourceString)
{
  loadRadioYamlStr(_radio_config);

#if defined(IMU)
  EXPECT_STREQ(getSourceString(MIXSRC_TILT_X), "TltX");
  EXPECT_STREQ(getSourceString(MIXSRC_TILT_Y), "TltY");
#endif

#if defined(PCBHORUS)
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_A), "smA");
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_B), "smB");
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_C), "smC");
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_D), "smD");
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_E), "smE");
  EXPECT_STREQ(getSourceString(MIXSRC_SPACEMOUSE_F), "smF");
#endif

  EXPECT_STREQ(getSourceString(MIXSRC_MIN), STR_MENU_MIN);
  EXPECT_STREQ(getSourceString(MIXSRC_MAX), STR_MENU_MAX);

  for (int source = MIXSRC_FIRST_RESERVED_TRIM; source <= MIXSRC_LAST_RESERVED_TRIM; ++source)
    EXPECT_STREQ(getSourceString(source), "---");
}
