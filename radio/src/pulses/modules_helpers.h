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

#include "definitions.h"
#include "edgetx_helpers.h"
#include "storage/storage.h"
#include "globals.h"
#include "telemetry/crossfire.h"

constexpr int CROSSFIRE_CHANNELS_COUNT = 16;
constexpr int8_t MAX_TRAINER_CHANNELS_M8 = MAX_TRAINER_CHANNELS - 8;
constexpr uint8_t MAX_RXNUM = 63;

inline bool isModuleCrossfire(uint8_t idx)
{
  return g_model.moduleData[idx].type == MODULE_TYPE_CROSSFIRE;
}

inline bool isModuleNone(uint8_t idx)
{
  return !isModuleCrossfire(idx);
}

inline bool isModuleELRS(uint8_t idx)
{
  return isModuleCrossfire(idx) && crossfireModuleStatus[idx].isELRS;
}

inline bool isInternalModuleCrossfire()
{
#if defined(INTERNAL_MODULE_CRSF)
  return g_eeGeneral.internalModule == MODULE_TYPE_CROSSFIRE;
#else
  return false;
#endif
}

inline int8_t maxModuleChannels(uint8_t idx)
{
  return isModuleCrossfire(idx) ? CROSSFIRE_CHANNELS_COUNT : 0;
}

inline int8_t maxModuleChannels_M8(uint8_t idx) { return maxModuleChannels(idx) - 8; }
inline int8_t minModuleChannels(uint8_t idx) { return maxModuleChannels(idx); }
inline int8_t defaultModuleChannels_M8(uint8_t idx) { return maxModuleChannels_M8(idx); }
inline int8_t sentModuleChannels(uint8_t idx) { return maxModuleChannels(idx); }
inline uint8_t getMaxRxNum(uint8_t) { return MAX_RXNUM; }

inline bool isModuleBindRangeAvailable(uint8_t idx)
{
  return isModuleELRS(idx) && CRSF_ELRS_MIN_VER(idx, 3, 4);
}

inline void setDefaultPpmFrameLengthTrainer()
{
  g_model.trainerData.frameLength = 4 * max<int>(0, g_model.trainerData.channelsCount);
}

void setModuleType(uint8_t moduleIdx, int moduleType);
bool isExternalAntennaEnabled();
