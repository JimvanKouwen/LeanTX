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

#include "edgetx.h"
#include "switches.h"

char* getModelAudioPath(char* path, bool trailingSlash)
{
  strcpy(path, SOUNDS_PATH "/");
  // sound files are stored under a lowercase language folder, e.g. SOUNDS/en
  memcpy(path + SOUNDS_PATH_LNG_OFS, "en", 2);
  char* buf = strcat_currentmodelname(path + sizeof(SOUNDS_PATH), ' ');

  if (!isFileAvailable(path)) {
    buf = strcat_currentmodelname(path + sizeof(SOUNDS_PATH), 0);
  }

  if (trailingSlash)
    *buf++ = '/';
  *buf = '\0';
  return buf;
}

static const char* const _sw_positions[] = {"-up", "-mid", "-down"};

bool getSwitchAudioFile(char* path, swsrc_t index)
{
  char* str = getModelAudioPath(path);

  if (index <= SWSRC_LAST_SWITCH) {
    div_t swinfo = switchInfo(index);
    auto sw_name = switchGetDefaultName(swinfo.quot);
    if (!sw_name) return false;
    str = strAppend(str, sw_name);
    str = strAppend(str, _sw_positions[swinfo.rem]);
  } else {
    div_t swinfo =
        div((int)(index - SWSRC_FIRST_MULTIPOS_SWITCH), XPOTS_MULTIPOS_COUNT);
    *str++ = 'S';
    *str++ = '1' + swinfo.quot;
    *str++ = '1' + swinfo.rem;
    *str = '\0';
  }
  strAppend(str, SOUNDS_EXT);
  return true;
}

bool matchSwitchAudioFile(const char* filename, int& sw_pos)
{
  // Switches Audio Files <switchname>-[up|mid|down].wav
  for (int i = 0; i < switchGetMaxAllSwitches(); i++) {
    auto* c = filename;
    auto sw_name = switchGetDefaultName(i);
    auto sw_name_len = strlen(sw_name);
    if (strncasecmp(c, sw_name, sw_name_len) != 0) continue;
    c += sw_name_len;
    for (size_t pos = 0; pos < DIM(_sw_positions); pos++) {
      auto pos_len = strlen(_sw_positions[pos]);
      if (strncasecmp(c, _sw_positions[pos], pos_len) != 0) continue;
      c += pos_len;
      if (*c != '.') continue;
      sw_pos = i * 3 + pos;
      return true;
    }
  }

  // match multipos switches
  {
    auto* c = filename;
    if (*c != 'S' && *c != 's') return false;
    c += 1;
    if (*c < '1' || *c > '9') return false;
    uint8_t xpot = uint8_t(*c++ - '1');
    if (*c < '1' || *c > '9') return false;
    uint8_t pos = uint8_t(*c++ - '1');
    if (pos >= XPOTS_MULTIPOS_COUNT) return false;
    if (*c != '.') return false;

    for (int i = 0; i < MAX_POTS; i++) {
      if (i != xpot) continue;
      if (!IS_POT_MULTIPOS(i)) continue;
      sw_pos = (MAX_SWITCHES * 3) + XPOTS_MULTIPOS_COUNT * xpot + pos;
      return true;
    }
  }

  return false;
}
