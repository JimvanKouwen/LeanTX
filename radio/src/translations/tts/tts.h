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

#include "audio.h"

#define PLAY_FUNCTION(x, ...) \
  void x(__VA_ARGS__, uint8_t id, int8_t fragmentVolume = USE_SETTINGS_VOLUME)

#define I18N_PLAY_FUNCTION(lng, x, ...)   \
  void lng##_##x(__VA_ARGS__, uint8_t id, \
                 int8_t fragmentVolume = USE_SETTINGS_VOLUME)

// Only English is shipped for now; declare the compiled-in voice functions
// directly here as translations are restored.
void en_playNumber(getvalue_t number, uint8_t unit, uint8_t flags, uint8_t id,
                   int8_t fragmentVolume);
void en_playDuration(int seconds, uint8_t flags, uint8_t id,
                     int8_t fragmentVolume);

inline PLAY_FUNCTION(playNumber, getvalue_t number, uint8_t unit, uint8_t flags) {
  en_playNumber(number, unit, flags, id, fragmentVolume);
}

inline PLAY_FUNCTION(playDuration, int seconds, uint8_t flags) {
  en_playDuration(seconds, flags, id, fragmentVolume);
}
