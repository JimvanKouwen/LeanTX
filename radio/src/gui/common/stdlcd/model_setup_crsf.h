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

static void editCrsfModuleType(uint8_t moduleIdx, coord_t y, LcdFlags attr, event_t event)
{
  lcdDrawTextIndented(y, STR_MODE);
  lcdDrawTextAtIndex(MODEL_SETUP_2ND_COLUMN, y, STR_MODULE_PROTOCOLS,
                     g_model.moduleData[moduleIdx].type, attr);
  if (attr && s_editMode > 0) {
    uint8_t type = checkIncDec(event, g_model.moduleData[moduleIdx].type,
                               MODULE_TYPE_NONE, MODULE_TYPE_MAX, EE_MODEL,
                               moduleIdx == INTERNAL_MODULE
                                   ? isInternalModuleAvailable : isExternalModuleAvailable);
    if (checkIncDec_Ret) setModuleType(moduleIdx, type);
  }
}

static void editCrsfChannels(uint8_t moduleIdx, coord_t y, LcdFlags attr, event_t event)
{
  auto& md = g_model.moduleData[moduleIdx];
  lcdDrawTextIndented(y, STR_CHANNELRANGE);
  lcdDrawText(MODEL_SETUP_2ND_COLUMN, y, STR_CH, attr);
  lcdDrawNumber(lcdLastRightPos, y, md.channelsStart + 1, LEFT | attr);
  lcdDrawChar(lcdLastRightPos, y, '-');
  lcdDrawNumber(lcdLastRightPos + FW, y, md.channelsStart + CROSSFIRE_CHANNELS_COUNT, LEFT);
  if (attr && s_editMode > 0)
    CHECK_INCDEC_MODELVAR_ZERO(event, md.channelsStart, MAX_OUTPUT_CHANNELS - CROSSFIRE_CHANNELS_COUNT);
}

static void editCrsfReceiver(uint8_t moduleIdx, coord_t y, LcdFlags attr, event_t event)
{
  lcdDrawTextIndented(y, STR_RECEIVER);
  lcdDrawNumber(MODEL_SETUP_2ND_COLUMN, y, g_model.header.modelId[moduleIdx],
                 (menuHorizontalPosition == 0 ? attr : 0) | LEADING0 | LEFT, 2);
  if (attr && menuHorizontalPosition == 0 && s_editMode > 0) {
    CHECK_INCDEC_MODELVAR_ZERO(event, g_model.header.modelId[moduleIdx], MAX_RXNUM);
    if (checkIncDec_Ret) {
      moduleState[moduleIdx].counter = CRSF_FRAME_MODELID;
      modelHeaders[g_eeGeneral.currModel].modelId[moduleIdx] = g_model.header.modelId[moduleIdx];
    }
  }
  if (isModuleBindRangeAvailable(moduleIdx)) {
    lcdDrawText(lcdNextPos + FW, y,
                 TELEMETRY_STREAMING() ? STR_MODULE_UNBIND : STR_MODULE_BIND,
                 menuHorizontalPosition == 1 ? attr : 0);
    if (attr && menuHorizontalPosition == 1 && event == EVT_KEY_BREAK(KEY_ENTER)) {
      moduleState[moduleIdx].mode = MODULE_MODE_BIND;
      AUDIO_PLAY(AU_SPECIAL_SOUND_CHEEP);
      s_editMode = 0;
    }
  }
}
