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

#include "channel_bar.h"

#include "bitmaps.h"
#include "etx_lv_theme.h"
#include "static.h"

ChannelBar::ChannelBar(Window* parent, const rect_t& rect, uint8_t channel,
                       std::function<int16_t()> getValueFunc, LcdColorIndex barColorIndex,
                       LcdColorIndex txtColorIndex) :
    Window(parent, rect), channel(channel),
    getValue(std::move(getValueFunc))
{
  setWindowFlag(NO_CLICK);

  etx_solid_bg(lvobj, COLOR_THEME_PRIMARY2_INDEX);

  bar = lv_obj_create(lvobj);
  etx_solid_bg(bar, barColorIndex);
  lv_obj_set_pos(bar, width() / 2, 0);
  lv_obj_set_size(bar, 0, height());

  coord_t yo = (height() < 10) ? -1 : -PAD_TINY;

  valText = etx_label_create(lvobj, FONT_XS_INDEX);
  lv_obj_set_pos(valText, width() / 2 + VAL_XO, yo);
  lv_obj_set_size(valText, VAL_W, VAL_H);
  etx_obj_add_style(valText, styles->text_align_left, LV_PART_MAIN);
  lv_obj_set_style_translate_x(valText, VAL_XT, LV_STATE_USER_1);
  etx_obj_add_style(valText, styles->text_align_right, LV_STATE_USER_1);
  etx_txt_color(valText, txtColorIndex);
  lv_label_set_text(valText, "");

  divPoints[0] = {(lv_coord_t)(width() / 2), 0};
  divPoints[1] = {(lv_coord_t)(width() / 2), (lv_coord_t)height()};

  lv_obj_t* divLine = lv_line_create(lvobj);
  etx_obj_add_style(divLine, styles->div_line, LV_PART_MAIN);
  lv_line_set_points(divLine, divPoints, 2);

  checkEvents();
}

void ChannelBar::checkEvents()
{
  Window::checkEvents();

  int newValue = getValue();

  if (value != newValue) {
    value = newValue;

    std::string s;
    if (g_eeGeneral.ppmunit == PPM_US)
      s = formatNumberAsString(PPM_CENTER + value / 2, 0, 0, "", STR_US);
    else if (g_eeGeneral.ppmunit == PPM_PERCENT_PREC1)
      s = formatNumberAsString(calcRESXto1000(value), PREC1, 0, "", "%");
    else
      s = formatNumberAsString(calcRESXto100(value), 0, 0, "", "%");

    if (s != valStr) {
      valStr = s;
      lv_label_set_text(valText, s.c_str());

      if (s[0] == '-')
        lv_obj_clear_state(valText, LV_STATE_USER_1);
      else
        lv_obj_add_state(valText, LV_STATE_USER_1);

      const int lim = RESX;
      int chanVal = limit<int>(-lim, value, lim);

      uint16_t size = divRoundClosest(abs(chanVal) * width(), lim * 2);

      int16_t x = width() / 2 - ((chanVal > 0) ? 0 : size);

      lv_obj_set_pos(bar, x, 0);
      lv_obj_set_size(bar, size, height());
    }

  }
}

//-----------------------------------------------------------------------------

MixerChannelBar::MixerChannelBar(Window* parent, const rect_t& rect,
                                 uint8_t channel) :
    ChannelBar(
        parent, rect, channel, [=] { return ex_chans[channel]; },
        COLOR_THEME_FOCUS_INDEX)
{
}

//-----------------------------------------------------------------------------

OutputChannelBar::OutputChannelBar(Window* parent, const rect_t& rect,
                                   uint8_t channel) :
    ChannelBar(parent, rect, channel,
               [=] { return channelOutputs[channel]; },
               COLOR_THEME_ACTIVE_INDEX)
{
}

//-----------------------------------------------------------------------------

ComboChannelBar::ComboChannelBar(Window* parent, const rect_t& rect,
                                 uint8_t _channel, bool isInHeader) :
    Window(parent, rect), channel(_channel)
{
  LcdColorIndex txtColIdx = isInHeader ? COLOR_THEME_PRIMARY2_INDEX : COLOR_THEME_SECONDARY1_INDEX;

  coord_t barW = width() - PAD_TINY;

  outputChannelBar = new OutputChannelBar(
      this, {PAD_TINY, ChannelBar::BAR_HEIGHT + PAD_TINY, barW, ChannelBar::BAR_HEIGHT},
      channel);

  new MixerChannelBar(
      this,
      {PAD_TINY, (2 * ChannelBar::BAR_HEIGHT) + PAD_TINY + 1, barW, ChannelBar::BAR_HEIGHT},
      channel);

  // Channel number
  char chanString[10];
  char* s = strAppend(chanString, STR_CH);
  strAppendSigned(s, channel + 1);
  new StaticText(this, {PAD_TINY, 0, LV_SIZE_CONTENT, ChannelBar::VAL_H}, chanString,
                 txtColIdx, FONT(XS) | LEFT);

  // Channel value in µS
  const char* suffix = (g_eeGeneral.ppmunit == PPM_US) ? "%" : STR_US;
  new DynamicNumber<int16_t>(
      this, {width() - ChannelBar::VAL_W, 0, ChannelBar::VAL_W, ChannelBar::VAL_H},
      [=] {
        if (g_eeGeneral.ppmunit == PPM_US)
          return calcRESXto100(channelOutputs[channel]);
        return PPM_CENTER + channelOutputs[channel] / 2;
      },
      txtColIdx, FONT(XS) | RIGHT, "", suffix);

}
