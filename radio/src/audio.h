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

#include <stddef.h>

#include "edgetx_types.h"
#include "dataconstants.h"

#include "hal/audio_driver.h"

// Longest model event filename: switch position.
constexpr uint8_t AUDIO_MODEL_FILENAME_MAXLEN = (sizeof("/SOUNDS/fr/") - 1) + LEN_MODEL_NAME + 1 + (sizeof("SA-down.wav") - 1);
constexpr uint8_t AUDIO_LUA_FILENAME_MAXLEN = 42; // Some scripts use long audio paths, even on 128x64 boards
constexpr uint8_t AUDIO_FILENAME_MAXLEN = (AUDIO_LUA_FILENAME_MAXLEN > AUDIO_MODEL_FILENAME_MAXLEN ? AUDIO_LUA_FILENAME_MAXLEN : AUDIO_MODEL_FILENAME_MAXLEN);

#define BEEP_MIN_FREQ                  (150)
#define BEEP_MAX_FREQ                  (15000)
#define BEEP_DEFAULT_FREQ              (2250)

#define USE_SETTINGS_VOLUME            (127)

// Requests are forwarded to the existing audio engine.
void audioPlayTone(uint16_t freq, uint16_t len, uint16_t pause=0, uint8_t flags=0,
                   int8_t freqIncr=0, int8_t fragmentVolume=USE_SETTINGS_VOLUME);
void audioPlayFile(const char *filename, uint8_t flags=0, uint8_t id=0,
                   int8_t fragmentVolume=USE_SETTINGS_VOLUME);
void audioStopPlay(uint8_t id);
void audioStopAll();
void audioFlush();
void audioStopSD();
bool audioIsPlaying(uint8_t id);

// Startup and audio-task integration; scheduling remains with the caller.
void audioStart();
bool audioStarted();
void audioWakeup();

extern uint8_t currentSpeakerVolume;

enum {
  // Prompt IDs used by audioEvent(); zero means an untagged sound.
  ID_PLAY_PROMPT_BASE = 1,
  ID_PLAY_FROM_SD_MANAGER = 255,
};

void codecsInit();
void audioEvent(unsigned int index);
void audioPlay(unsigned int index, uint8_t id=0);

void onKeyError();

void audioKeyPress();
void audioKeyError();
void audioTimerCountdown(uint8_t timer, int value);

#if defined(AUDIO)

#define AUDIO_ERROR_MESSAGE(e)   audioEvent(e)
#define AUDIO_TIMER_MINUTE(t)    playDuration(t, 0, 0)

#define AUDIO_KEY_PRESS()        audioKeyPress()
#define AUDIO_KEY_ERROR()        audioKeyError()

#define AUDIO_HELLO()            audioPlay(AUDIO_HELLO)
#define AUDIO_BYE()              audioPlay(AU_BYE, ID_PLAY_PROMPT_BASE + AU_BYE)
#define AUDIO_WARNING1()         audioEvent(AU_WARNING1)
#define AUDIO_WARNING2()         audioEvent(AU_WARNING2)
#define AUDIO_TX_BATTERY_LOW()   audioEvent(AU_TX_BATTERY_LOW)
#define AUDIO_ERROR()            audioEvent(AU_ERROR)
#define AUDIO_TIMER_COUNTDOWN(idx, val) audioTimerCountdown(idx, val)
#define AUDIO_TIMER_ELAPSED(idx) audioEvent(AU_TIMER1_ELAPSED+idx)
#define AUDIO_INACTIVITY()       audioEvent(AU_INACTIVITY)
#define AUDIO_POT_MIDDLE(x)      audioEvent(AU_STICK1_MIDDLE+x)
#define AUDIO_PLAY(p)            audioEvent(p)
#define AUDIO_VARIO(fq, t, p, f) audioPlayTone(fq, t, p, f)
#define AUDIO_RSSI_ORANGE()      audioEvent(AU_RSSI_ORANGE)
#define AUDIO_RSSI_RED()         audioEvent(AU_RSSI_RED)
#define AUDIO_TELEMETRY_CONNECTED() audioEvent(AU_TELEMETRY_CONNECTED)
#define AUDIO_TELEMETRY_LOST()   audioEvent(AU_TELEMETRY_LOST)
#define AUDIO_TELEMETRY_BACK()   audioEvent(AU_TELEMETRY_BACK)

#else // AUDIO

#define AUDIO_TIMER_COUNTDOWN(idx, val)
#define AUDIO_TIMER_ELAPSED(idx)
#define AUDIO_VARIO(fq, t, p, f)
#define AUDIO_RSSI_ORANGE()
#define AUDIO_RSSI_RED()
#define AUDIO_TELEMETRY_CONNECTED()
#define AUDIO_TELEMETRY_LOST()
#define AUDIO_TELEMETRY_BACK()

#endif

enum AutomaticPromptsCategories {
  SYSTEM_AUDIO_CATEGORY,
  MODEL_AUDIO_CATEGORY,
  SWITCH_AUDIO_CATEGORY,
};

void pushPrompt(uint16_t prompt, uint8_t id=0, uint8_t fragmentVolume = USE_SETTINGS_VOLUME);
void pushUnit(uint8_t unit, uint8_t idx, uint8_t id, uint8_t fragmentVolume = USE_SETTINGS_VOLUME);
void playModelName();

#define PUSH_NUMBER_PROMPT(p)    pushPrompt((p), id, fragmentVolume)
#define PUSH_UNIT_PROMPT(p, i)   pushUnit((p), (i), id, fragmentVolume)
#define PLAY_NUMBER(n, u, a)     playNumber((n), (u), (a), id, fragmentVolume)
#define PLAY_DURATION(d, att)    playDuration((d), (att), id, fragmentVolume)
#define PLAY_DURATION_ATT        , uint8_t flags
#define PLAY_TIME                1
#define PLAY_LONG_TIMER          2
#define LONG_TIMER_DURATION      (10 * 60)  // 10 minutes
#define IS_PLAY_TIME()           (flags & PLAY_TIME)
#define IS_PLAY_LONG_TIMER()     (flags & PLAY_LONG_TIMER)
#define IS_PLAYING(id)           audioIsPlaying((id))
#define PLAY_FILE(f, flags, id)  audioPlayFile((f), (flags), (id), USE_SETTINGS_VOLUME)
#define STOP_PLAY(id)            audioStopPlay((id))

#if defined(AUDIO)
#define AUDIO_RESET()            audioStopAll()
#define AUDIO_FLUSH()            audioFlush()
#endif

#if defined(AUDIO)
  extern tmr10ms_t timeAutomaticPromptsSilence;
  void playModelEvent(uint8_t category, uint8_t index, event_t event=0);
  #define PLAY_SWITCH_MOVED(sw, pos)    playModelEvent(SWITCH_AUDIO_CATEGORY, (sw) * 3 + (pos))
  #define PLAY_MULTIPOS_MOVED(pot, pos) playModelEvent(SWITCH_AUDIO_CATEGORY, MAX_SWITCHES * 3 + (pot) * XPOTS_MULTIPOS_COUNT + (pos))
  #define PLAY_MODEL_NAME()             playModelName()
  #define START_SILENCE_PERIOD()        timeAutomaticPromptsSilence = get_tmr10ms()
  #define IS_SILENCE_PERIOD_ELAPSED()   (get_tmr10ms()-timeAutomaticPromptsSilence > 50)
#else
  #define PLAY_SWITCH_MOVED(sw, pos)
  #define PLAY_MULTIPOS_MOVED(pot, pos)
  #define PLAY_MODEL_NAME()
  #define START_SILENCE_PERIOD()
  #define IS_SILENCE_PERIOD_ELAPSED()   true
#endif

char * getAudioPath(char * path);

void referenceSystemAudioFiles();
void referenceModelAudioFiles();

bool isAudioFileReferenced(uint32_t i, char * filename/*at least AUDIO_FILENAME_MAXLEN+1 long*/);
