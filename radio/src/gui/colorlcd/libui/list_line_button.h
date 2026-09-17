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

#include "button.h"
#include "edgetx_types.h"
#include "pagegroup.h"

class ListLineButton : public ButtonBase
{
 public:
  ListLineButton(Window* parent, uint8_t index);

  uint8_t getIndex() const { return index; }
  virtual void setIndex(uint8_t i) { index = i; }

  void checkEvents() override;

  virtual void refresh() = 0;

  static constexpr coord_t BTN_H = EdgeTxStyles::STD_FONT_HEIGHT + PAD_BORDER * 2 + PAD_OUTLINE * 2;
  static constexpr coord_t GRP_W = LCD_W - PAD_SMALL * 2;

 protected:
  uint8_t index;

  virtual bool isActive() const { return false; }
};
