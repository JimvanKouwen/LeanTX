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
#include "dataconstants.h"
#include "edgetx_types.h"

PACK(struct GlobalData {
  uint8_t externalAntennaEnabled:1;
  uint8_t authenticationCount:2;
  uint8_t upgradeModulePopup:1;
  uint8_t internalModuleVersionChecked:1;
  uint8_t spare:3;
});

extern GlobalData globalData;

extern uint16_t sessionTimer;

extern uint32_t maxMixerDuration;

#if defined(AUDIO)
extern uint8_t requiredSpeakerVolume;
#endif

extern uint8_t requiredBacklightBright;

extern int16_t channelOutputs[MAX_OUTPUT_CHANNELS];

typedef uint16_t BeepANACenter;
extern BeepANACenter bpanaCenter;

extern uint8_t controlsInitialized;

extern int16_t calibratedAnalogs[MAX_ANALOG_INPUTS];

extern uint8_t g_beepCnt;
extern uint8_t beepAgain;
extern uint16_t lightOffCounter;
extern uint8_t flashCounter;
