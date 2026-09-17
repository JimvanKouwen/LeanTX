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

typedef int choice_t;

choice_t editChoice(coord_t x, coord_t y, const char *label,
                    const char *const *values, choice_t value, choice_t min,
                    choice_t max, LcdFlags attr, event_t event);
choice_t editChoice(coord_t x, coord_t y, const char *label,
                    const char *const *values, choice_t value, choice_t min,
                    choice_t max, LcdFlags attr, event_t event, coord_t lblX);
choice_t editChoice(coord_t x, coord_t y, const char *label,
                    const char *const *values, choice_t value, choice_t min,
                    choice_t max, LcdFlags attr, event_t event, coord_t lblX,
                    IsValueAvailable isValueAvailable);

uint8_t editCheckBox(uint8_t value, coord_t x, coord_t y, const char *label,
                     LcdFlags attr, event_t event);
uint8_t editCheckBox(uint8_t value, coord_t x, coord_t y, const char *label,
                     LcdFlags attr, event_t event, coord_t lblX);


extern uint8_t editNameCursorPos;

void editName(coord_t x, coord_t y, char *name, uint8_t size, event_t event,
              uint8_t active, LcdFlags attr, uint8_t old_editMode);

void editSingleName(coord_t x, coord_t y, const char *label, char *name,
                    uint8_t size, event_t event, uint8_t active,
                    uint8_t old_editMode, coord_t lblX = 0);

int editNumberField(const char* name, coord_t lx, coord_t vx, coord_t y, int val,
                    int min, int max, LcdFlags attr, event_t event, const char* zeroStr = nullptr, int ofst = 0);
