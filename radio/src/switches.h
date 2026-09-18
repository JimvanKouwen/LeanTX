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

#include <stdint.h>
#include <stdlib.h>

#include "edgetx_types.h"
#include "edgetx_constants.h"

bool isSwitchWarningRequired(uint16_t &bad_pots);

void getSwitchesPosition(bool startup);

uint8_t getSwitchCount();

uint8_t switchGetMaxRow(uint8_t col);


bool getSwitch(swsrc_t swtch);
uint8_t getXPotPosition(uint8_t idx);

div_t switchInfo(int switchPosition);

// Lookup switch index by letter ('A' to 'Z')
// note: no customizable switches
int switchLookupIdx(char c);

// Lookup switch index by name ('SA' to 'SZ')
// note: no customizable switches
int switchLookupIdx(const char* name, size_t len);

// Get switch letter ('A' to 'Z')
// note: no customizable switches
char switchGetLetter(uint8_t idx);

// customizable switches supported
uint8_t getSwitchCountInFSGroup(uint8_t index);

SwitchConfig switchGetMaxType(uint8_t idx);

#if defined(FUNCTION_SWITCHES)
void setFSStartupPosition();
void evalFunctionSwitches();
void setFSLogicalState(uint8_t index, uint8_t value);
bool groupHasSwitchOn(uint8_t group);
int firstSwitchInGroup(uint8_t group);
int groupDefaultSwitch(uint8_t group);
void setGroupSwitchState(uint8_t group);
bool getFSPhysicalState(uint8_t index);

//led_driver.cpp
void fsLedOff(uint8_t index);
void fsLedOn(uint8_t index);
bool fsLedState(uint8_t index);
void fsLedRGB(uint8_t index, uint32_t color);
uint32_t fsGetLedRGB(uint8_t index);
uint8_t getRGBColorIndex(uint32_t color);
#endif

void setAllPreflightSwitchStates();
