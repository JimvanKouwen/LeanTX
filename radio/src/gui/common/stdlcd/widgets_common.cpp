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

#include "edgetx.h"

choice_t editChoice(coord_t x, coord_t y, const char * label, const char *const *values, choice_t value, choice_t min, choice_t max, LcdFlags attr, event_t event, coord_t lblX, IsValueAvailable isValueAvailable)
{
  if (label) {
    lcdDrawText(lblX, y, label);
  }
  if (values) lcdDrawTextAtIndex(x, y, values, value-min, attr);
  if (attr & (~RIGHT)) value = checkIncDec(event, value, min, max, (isModelMenuDisplayed()) ? EE_MODEL : EE_GENERAL, isValueAvailable);
  return value;
}

choice_t editChoice(coord_t x, coord_t y, const char * label, const char *const *values, choice_t value, choice_t min, choice_t max, LcdFlags attr, event_t event, coord_t lblX)
{
  return editChoice(x, y, label, values, value, min, max, attr, event, lblX, nullptr);
}

choice_t editChoice(coord_t x, coord_t y, const char * label, const char *const *values, choice_t value, choice_t min, choice_t max, LcdFlags attr, event_t event)
{
  return editChoice(x, y, label, values, value, min, max, attr, event, 0, nullptr);
}

uint8_t editCheckBox(uint8_t value, coord_t x, coord_t y, const char *label, LcdFlags attr, event_t event, coord_t lblX)
{
  drawCheckBox(x, y, value, attr);
  return editChoice(x, y, label, nullptr, value, 0, 1, attr, event, lblX);
}

uint8_t editCheckBox(uint8_t value, coord_t x, coord_t y, const char *label, LcdFlags attr, event_t event)
{
  return editCheckBox(value, x, y, label, attr, event, 0);
}

swsrc_t editSwitch(coord_t x, coord_t y, swsrc_t value, LcdFlags attr, event_t event)
{
  lcdDrawTextAlignedLeft(y, STR_SWITCH);
  drawSwitch(x,  y, value, attr);
  if (attr & (~RIGHT)) CHECK_INCDEC_MODELSWITCH(event, value, SWSRC_FIRST_IN_MIXES, SWSRC_LAST_IN_MIXES, isSwitchAvailableInMixes);
  return value;
}

int16_t editLiteralFieldValue(coord_t x, coord_t y, const char* title, int16_t value,
                              int16_t min, int16_t max, LcdFlags attr, event_t event)
{

  if (title) lcdDrawTextAlignedLeft(y, title);
  lcdDrawNumber(x, y, value, attr);
  if (attr & (~RIGHT))
    value = checkIncDec(event, value, min, max, EE_MODEL | NO_INCDEC_MARKS);
  return value;

}

void editSingleName(coord_t x, coord_t y, const char * label, char *name, uint8_t size, event_t event, uint8_t active, uint8_t old_editMode, coord_t lblX)
{
  lcdDrawText(lblX, y, label);
  editName(x, y, name, size, event, active, 0, old_editMode);
}

int editNumberField(const char* name, coord_t lx, coord_t vx, coord_t y, int val,
                    int min, int max, LcdFlags attr, event_t event, const char* zeroStr, int ofst)
{
  lcdDrawText(lx, y, name);
  if (zeroStr && val == 0)
    lcdDrawText(vx, y, zeroStr, attr & (~PREC1));
  else
    lcdDrawNumber(vx, y, val + ofst, LEFT|attr);
  if (attr & INVERS) CHECK_INCDEC_MODELVAR(event, val, min, max);
  return val;
}
