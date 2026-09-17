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

#include <functional>
#include "lcd.h"
#include "keys.h"
#include "telemetry/telemetry_sensors.h"

#define READONLY_ROW                   ((uint8_t)-1)
#define TITLE_ROW                      READONLY_ROW
#define LABEL(...)                     READONLY_ROW
#define HIDDEN_ROW                     ((uint8_t)-2)

#if defined(ROTARY_ENCODER_NAVIGATION)
  #define CASE_ROTARY_ENCODER(x) x,
#else
  #define CASE_ROTARY_ENCODER(x)
#endif

#if defined(COLORLCD)
typedef std::function<bool(int)> IsValueAvailable;
#else
typedef bool (*IsValueAvailable)(int);
#endif

enum SwitchContext
{
  AllSwitchesContext,
  TimerSwitchContext
};

int getFirstAvailable(int min, int max, IsValueAvailable isValueAvailable);

bool isChannelUsed(int channel);
int getChannelsUsed();
bool checkSourceAvailable(int source, uint32_t sourceTypes);
bool checkSwitchAvailable(int swtch, uint32_t swtchTypes);
bool isThrottleSourceAvailable(int source);
bool isSourceAvailableForBacklightOrVolume(int source);
bool isSourceAvailable(int source);
bool isMixerSourceAvailable(int source);
int timersSetupCount();
bool isTimerSourceAvailable(int source);
bool isSwitchAvailable(int swtch, SwitchContext context);
bool isSerialModeAvailable(uint8_t port_nr, int mode);
bool isSerialModeAvailable(uint8_t port_nr, int mode, const RadioData& settings);
bool isSwitchAvailableForArming(int swtch);
bool isTimerSwitchAvailable(int swtch);
bool isExternalModuleAvailable(int moduleType);
bool isInternalModuleAvailable(int moduleType);
bool isInternalModuleSupported(int moduleType);
bool isPotTypeAvailable(uint8_t type);
bool isFlexSwitchSourceValid(int source);
bool getPotInversion(int index);
bool getStickInversion(int index);
void setStickInversion(int index, bool value);
void setPotInversion(int index, bool value);
uint8_t getPotType(int index);
void setPotType(int index, int value);

bool isSensorUnit(int sensor, uint8_t unit);
bool isCellsSensor(int sensor);
bool isGPSSensor(int sensor);
bool isAltSensor(int sensor);
bool isVoltsSensor(int sensor);
bool isCurrentSensor(int sensor);
bool isTelemetryFieldAvailable(int index);
uint8_t getTelemetrySensorsCount();
bool isTelemetryFieldComparisonAvailable(int index);
bool isSensorAvailable(int sensor);
bool isVarioSensorAvailable(int sensor);

bool modelHasNotes();

bool confirmModelChange();

#if defined(COLORLCD)
bool isSwitch2POSWarningStateAvailable(int state);
#endif

swsrc_t checkIncDecMovedSwitch(swsrc_t val);

// TODO move this to stdlcd/draw_functions.h ?
void drawCurveRef(coord_t x, coord_t y, CurveRef & curve, LcdFlags flags=0);
void drawDate(coord_t x, coord_t y, TelemetryItem & telemetryItem, LcdFlags flags=0);
void drawTelemScreenDate(coord_t x, coord_t y, source_t sensor, LcdFlags flags=0);
void drawGPSPosition(coord_t x, coord_t y, int32_t longitude, int32_t latitude, LcdFlags flags=0);
void drawGPSSensorValue(coord_t x, coord_t y, TelemetryItem & telemetryItem, LcdFlags flags=0);
void drawSensorCustomValue(coord_t x, coord_t y, uint8_t sensor, int32_t value, LcdFlags flags=0);
void drawSourceCustomValue(coord_t x, coord_t y, mixsrc_t channel, int32_t val, LcdFlags flags=0);
void drawSourceValue(coord_t x, coord_t y, source_t channel, LcdFlags flags=0);

// model_setup Defines that are used in all uis in the same way
#define IF_INTERNAL_MODULE_ON(x)                  (IS_INTERNAL_MODULE_ENABLED() ? (uint8_t)(x) : HIDDEN_ROW)
#define IF_MODULE_ON(moduleIndex, x)              (IS_MODULE_ENABLED(moduleIndex) ? (uint8_t)(x) : HIDDEN_ROW)

extern uint8_t MODULE_BIND_ROWS(int moduleIdx);
extern uint8_t MODULE_CHANNELS_ROWS(int moduleIdx);

#if defined(EXTERNAL_ANTENNA)
void checkExternalAntenna();
#if defined(COLORLCD)
void setAntennaModeWithConfirm(int8_t newMode, uint8_t storageId,
                                std::function<void(int8_t)> setter);
#endif
#endif

void editStickHardwareSettings(coord_t x, coord_t y, int idx, event_t event,
                               LcdFlags flags, uint8_t old_editMode);

const char * getMultiOptionTitleStatic(uint8_t moduleIdx);
const char *getMultiOptionTitle(uint8_t moduleIdx);

void writeScreenshot();

uint8_t expandableSection(coord_t y, const char* title, uint8_t value, uint8_t attr, event_t event);
