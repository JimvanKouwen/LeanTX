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

#include "mixer_edit_adv.h"

#include "choice.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "getset_helpers.h"
#include "mixes.h"
#include "numberedit.h"
#include "toggleswitch.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

MixEditAdvanced::MixEditAdvanced(int8_t channel, uint8_t index) :
    Page(ICON_MODEL_MIXER, PAD_MEDIUM), channel(channel), index(index)
{
  std::string title2(getSourceString(MIXSRC_FIRST_CH + channel));
  header->setTitle(STR_MIXES);
  header->setTitle2(title2);

  buildBody(body);
}

#if !NARROW_LAYOUT
static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                     LV_GRID_FR(1), LV_GRID_FR(1),
                                     LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
#else
static const lv_coord_t col_dsc[] = {LV_GRID_FR(12), LV_GRID_FR(13),
                                     LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT,
                                     LV_GRID_TEMPLATE_LAST};
#endif

void MixEditAdvanced::buildBody(Window* form)
{
  FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);
  form->setFlexLayout();

  MixData* mix = mixAddress(index);

  // Advanced...
  FormLine* line;

  // Multiplex
  if (index > 0 && mixAddress(index - 1)->destCh == channel) {
    line = form->newLine(grid);
    new StaticText(line, rect_t{}, STR_MULTPX);
    new Choice(line, rect_t{}, STR_VMLTPX, 0, 2, GET_SET_DEFAULT(mix->mltpx));
  }

  line = form->newLine(grid);
  // Warning
  new StaticText(line, rect_t{}, STR_MIXWARNING);
  auto edit = new NumberEdit(line, rect_t{}, 0, 3,
                             GET_SET_DEFAULT(mix->mixWarn));
  edit->setZeroText(STR_OFF);

}
