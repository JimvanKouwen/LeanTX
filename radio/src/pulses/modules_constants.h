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

#include "storage/yaml/yaml_defs.h"

enum ModuleType {
  MODULE_TYPE_NONE = 0,
  MODULE_TYPE_CROSSFIRE = 5,
  MODULE_TYPE_COUNT SKIP = 6,
  MODULE_TYPE_MAX SKIP = MODULE_TYPE_CROSSFIRE
};

enum AntennaModes {
  ANTENNA_MODE_INTERNAL = -2,
  ANTENNA_MODE_ASK = -1,
  ANTENNA_MODE_PER_MODEL = 0,
  ANTENNA_MODE_EXTERNAL = 1,
  ANTENNA_MODE_FIRST SKIP = ANTENNA_MODE_INTERNAL,
  ANTENNA_MODE_LAST SKIP = ANTENNA_MODE_EXTERNAL
};
