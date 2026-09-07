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

#if defined(ALL_LANGS)
#include <functional>
#endif

#include "audio.h"

#if !defined(ALL_LANGS)
struct LanguagePack {
  const char* id;
  const char* name;
  void (*playNumber)(getvalue_t number, uint8_t unit, uint8_t flags, uint8_t id, int8_t fragmentVolume);
  void (*playDuration)(int seconds, uint8_t flags, uint8_t id, int8_t fragmentVolume);
};
#else
struct LanguagePack {
  const char* id;
  const char* (*name)();
  void (*playNumber)(getvalue_t number, uint8_t unit, uint8_t flags, uint8_t id, int8_t fragmentVolume);
  void (*playDuration)(int seconds, uint8_t flags, uint8_t id, int8_t fragmentVolume);
};
#endif

extern const LanguagePack * currentLanguagePack;
extern uint8_t currentLanguagePackIdx;
extern uint8_t getLanguageId(const char* lang);

// Only English is shipped for now; re-add language codes here as translations are restored.
enum RadioLanguage {
  LANG_EN,
  LANG_COUNT
};

extern const LanguagePack enLanguagePack;
extern const LanguagePack * const languagePacks[];

#if defined(LANGUAGE_PACKS_DEFINITION)
const LanguagePack * const languagePacks[] = {
  &enLanguagePack,
  NULL
};
#endif

#if !defined(ALL_LANGS)
#define LANGUAGE_PACK_DECLARE(lng, name) \
  const LanguagePack lng##LanguagePack = {#lng, name, lng##_playNumber, lng##_playDuration}
#else
#define LANGUAGE_PACK_DECLARE(lng, name) \
  const char* lng##_nameFunc() { return name; }; \
  const LanguagePack lng##LanguagePack = {#lng, lng##_nameFunc, lng##_playNumber, \
                                          lng##_playDuration}
#endif

#define LANGUAGE_PACK_DECLARE_DEFAULT(lng, name)                \
  LANGUAGE_PACK_DECLARE(lng, name);                             \
  const LanguagePack* currentLanguagePack = &lng##LanguagePack; \
  uint8_t currentLanguagePackIdx

#define PLAY_FUNCTION(x, ...)    void x(__VA_ARGS__, uint8_t id, int8_t fragmentVolume = USE_SETTINGS_VOLUME)

inline PLAY_FUNCTION(playNumber, getvalue_t number, uint8_t unit, uint8_t flags) {
  currentLanguagePack->playNumber(number, unit, flags, id, fragmentVolume);
}

inline PLAY_FUNCTION(playDuration, int seconds, uint8_t flags) {
   currentLanguagePack->playDuration(seconds, flags, id, fragmentVolume);
}

#define I18N_PLAY_FUNCTION(lng, x, ...) void lng ## _ ## x(__VA_ARGS__, uint8_t id, int8_t fragmentVolume = USE_SETTINGS_VOLUME)
