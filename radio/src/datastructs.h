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

#if !defined(BOOT)
#include "datastructs_private.h"

#if !defined(BACKUP)
/* Compile time check to test structure size has not changed *
   Changing the size of one of the eeprom structs may cause wrong data to
   be loaded. Error out if the struct size changes.
   This function tries not avoid checking or using the defines
   other than the CPU arch and board type so changes in other
   defines also trigger the struct size changes */

#include "chksize.h"

#define CHKSIZE(x, y) check_size<struct x, y>()
#define CHKTYPE(x, y) check_size<x, y>()

static inline void check_struct()
{
  CHKSIZE(CurveRef, 2);
  CHKSIZE(VarioData, 5);
  CHKSIZE(MixData, 19);
  CHKSIZE(ExpoData, 17);
  CHKSIZE(CurveHeader, 4);
  CHKSIZE(LogicalSwitchData, 9);
  CHKSIZE(TelemetrySensor, 14);
  CHKSIZE(ModuleData, 6);
  CHKSIZE(RFAlarmData, 2);
  CHKSIZE(CustomFunctionData, 11);

#if defined(PCBX7)
  CHKSIZE(LimitData, 11);
  CHKSIZE(TimerData, 12);
  CHKSIZE(TelemetryBarData, 6);
  CHKSIZE(TelemetryLineData, 4);
  CHKTYPE(TelemetryScreenData, 24);
  CHKSIZE(ModelHeader, 12);
#elif defined(PCBTARANIS)
  CHKSIZE(LimitData, 13);
  CHKSIZE(TimerData, 17);
  CHKSIZE(TelemetryBarData, 6);
  CHKSIZE(TelemetryLineData, 6);
  CHKTYPE(TelemetryScreenData, 24);
  CHKSIZE(ModelHeader, 24);
#elif defined(COLORLCD)
  CHKSIZE(LimitData, 13);
  CHKSIZE(TimerData, 17);
  CHKSIZE(ModelHeader, 131);
#else
  #error CHKSIZE not set up
#endif

#if defined(RADIO_ST16) || defined(PCBPA01) || defined(RADIO_TX15) || defined(RADIO_GX15) || defined(RADIO_T15PRO) || defined(RADIO_TX16SMK3) || defined(RADIO_T22)
  CHKSIZE(RadioData, 1158);
#elif defined(RADIO_V12)
  CHKSIZE(RadioData, 1155);
#elif defined(COLORLCD)
  #if defined(IMU)
    CHKSIZE(RadioData, 1038);
  #else
    CHKSIZE(RadioData, 1037);
  #endif
#elif defined(RADIO_GX12)
  CHKSIZE(RadioData, 1043);
#else
  CHKSIZE(RadioData, 923);
#endif

#if defined(RADIO_TPROV2) || defined(RADIO_BUMBLEBEE)
  CHKSIZE(ModelData, 5672);
#elif defined(RADIO_FAMILY_T20)
  CHKSIZE(ModelData, 5672);
#elif defined(RADIO_GX12)
  CHKSIZE(ModelData, 5736);
#elif defined(PCBX9E)
    CHKSIZE(ModelData, 6052);
#elif defined(PCBX9DP)
  CHKSIZE(ModelData, 6051);
#elif defined(PCBX7) || \
    defined(RADIO_T14) || defined(RADIO_T12MAX)
  CHKSIZE(ModelData, 5646);
#elif defined(PCBPL18)
#if defined(RADIO_NV14_FAMILY)
  CHKSIZE(ModelData, 6120);
#else
  CHKSIZE(ModelData, 6122);
#endif
#elif defined(PCBST16) || defined(RADIO_T15PRO) || defined(RADIO_TX15) || defined(RADIO_GX15)
  CHKSIZE(ModelData, 6736);
#elif defined(RADIO_V12)
  CHKSIZE(ModelData, 6735);
#elif defined(PCBC14)
  CHKSIZE(ModelData, 6668);
#elif defined(PCBPA01)
  CHKSIZE(ModelData, 6713);
#elif defined(RADIO_T15)
  CHKSIZE(ModelData, 6148);
#elif defined(RADIO_T22)
  CHKSIZE(ModelData, 6736);
#elif defined(RADIO_TX16SMK3)
  CHKSIZE(ModelData, 6737);
#elif defined(RADIO_H7RS)
  // CHKSIZE()
#elif defined(PCBHORUS)
  CHKSIZE(ModelData, 6122);
#else
  #error CHKSIZE not set up
#endif

#undef CHKSIZE
}
#endif /* BACKUP */
#endif /* !BOOT */
