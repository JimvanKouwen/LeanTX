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

#pragma once

static const char * const options[] = {
#if defined(AUTOUPDATE)
  "autoupdate",
#endif
#if defined(BLUETOOTH)
  "bluetooth",
#endif
#if defined(CROSSFIRE)
  "crossfire",
#endif
  "eu",
#if defined(FAI)
  "FAImode",
#endif
#if defined(FAI_CHOICE)
  "FAIchoice",
#endif
#if defined(HORUS_STICKS)
  "horussticks",
#endif
#if defined(INTERNAL_GPS)
  "internalgps",
#endif
#if defined(SPACEMOUSE)
  "spacemouse",
#endif
#if defined(LUA_COMPILER)
  "luac",
#endif
#if defined(IMU_LSM6DS33)
  "lsm6ds33",
#endif
#if defined(CLI)
    "cli",
#endif
#if defined(ENABLE_SERIAL_PASSTHROUGH)
    "passthrough",
#endif
  nullptr //sentinel
};
