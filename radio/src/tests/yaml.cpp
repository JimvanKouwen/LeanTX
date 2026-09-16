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
#include "model_yaml_test.h"

TEST(Yaml, SkipBlankLinesAndReadFinalUnterminatedLine)
{
  loadModelYamlStr("header:\n  name: Test\n\ntimers:\n  0:\n    start: 34\n\n    value: 45");
  EXPECT_STREQ("Test", g_model.header.name);
  EXPECT_EQ(34, g_model.timers[0].start);
  EXPECT_EQ(45, g_model.timers[0].value);
}
