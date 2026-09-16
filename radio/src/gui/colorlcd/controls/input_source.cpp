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

#include "input_source.h"

#include "edgetx.h"
#include "getset_helpers.h"
#include "sourcechoice.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

InputSource::InputSource(Window *parent, ExpoData *input) :
    Window(parent, rect_t{})
{
  padAll(PAD_TINY);
  lv_obj_set_flex_flow(lvobj, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_size(lvobj, lv_pct(100), LV_SIZE_CONTENT);

  new SourceChoice(
      this, rect_t{}, INPUTSRC_FIRST, INPUTSRC_LAST, GET_DEFAULT(input->srcRaw),
      [=](int32_t newValue) {
        input->srcRaw = newValue;
        SET_DIRTY();
      }, true);

}
