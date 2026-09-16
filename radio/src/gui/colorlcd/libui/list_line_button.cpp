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

#include "list_line_button.h"

#include <algorithm>

#include "edgetx.h"
#include "etx_lv_theme.h"

static void mix_line_constructor(const lv_obj_class_t* class_p,
                                       lv_obj_t* obj)
{
  etx_std_style(obj, LV_PART_MAIN, PAD_TINY);
}

static const lv_obj_class_t mix_line_class = {
    .base_class = &lv_btn_class,
    .constructor_cb = mix_line_constructor,
    .destructor_cb = nullptr,
    .user_data = nullptr,
    .event_cb = nullptr,
    .width_def = ListLineButton::GRP_W,
    .height_def = ListLineButton::BTN_H,
    .editable = LV_OBJ_CLASS_EDITABLE_INHERIT,
    .group_def = LV_OBJ_CLASS_GROUP_DEF_TRUE,
    .instance_size = sizeof(lv_btn_t),
};

static lv_obj_t* mix_line_create(lv_obj_t* parent)
{
  return etx_create(&mix_line_class, parent);
}

ListLineButton::ListLineButton(Window* parent, uint8_t index) :
    ButtonBase(parent, rect_t{}, nullptr, mix_line_create), index(index)
{
}

void ListLineButton::checkEvents()
{
  check(isActive());
  ButtonBase::checkEvents();
}

MixButtonBase::MixButtonBase(Window* parent, uint8_t index) :
    ListLineButton(parent, index)
{
  setWidth(BTN_W);
  setHeight(ListLineButton::BTN_H);
  padAll(PAD_ZERO);
}

void MixButtonBase::setWeight(int16_t value, int16_t min, int16_t max)
{
  if (!weight) {
    weight = etx_label_create(lvobj);
    lv_obj_set_pos(weight, WGT_X, WGT_Y);
    lv_obj_set_size(weight, WGT_W, WGT_H);
    etx_font(weight, FONT_XS_INDEX, LV_STATE_USER_1);
  }

  char s[32];
  formatConfigValue(s, sizeof(s), value, 0, "%");
  if (getTextWidth(s, 0, FONT(STD)) > WGT_W)
    lv_obj_add_state(weight, LV_STATE_USER_1);
  else
    lv_obj_clear_state(weight, LV_STATE_USER_1);

  lv_label_set_text(weight, s);
}

void MixButtonBase::setSource(mixsrc_t idx)
{
  if (!source) {
    source = etx_label_create(lvobj);
    lv_obj_set_pos(source, SRC_X, SRC_Y);
    lv_obj_set_size(source, SRC_W, SRC_H);
    etx_font(source, FONT_XS_INDEX, LV_STATE_USER_1);
  }

  char* s = getSourceString(idx);
  if (getTextWidth(s, 0, FONT(STD)) > SRC_W)
    lv_obj_add_state(source, LV_STATE_USER_1);
  else
    lv_obj_clear_state(source, LV_STATE_USER_1);

  lv_label_set_text(source, s);
}

void MixButtonBase::setOpts(const char* s)
{
  if (!opts) {
    opts = etx_label_create(lvobj);
    lv_obj_set_pos(opts, OPT_X, OPT_Y);
    lv_obj_set_size(opts, OPT_W, OPT_H);
    etx_font(opts, FONT_XS_INDEX, LV_STATE_USER_1);
  }

  if (getTextWidth(s, 0, FONT(STD)) > OPT_W)
    lv_obj_add_state(opts, LV_STATE_USER_1);
  else
    lv_obj_clear_state(opts, LV_STATE_USER_1);

  lv_label_set_text(opts, s);
}

static void group_constructor(const lv_obj_class_t* class_p, lv_obj_t* obj)
{
  etx_std_style(obj, LV_PART_MAIN, PAD_TINY);
}

static const lv_obj_class_t group_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = group_constructor,
    .destructor_cb = nullptr,
    .user_data = nullptr,
    .event_cb = nullptr,
    .width_def = ListLineButton::GRP_W,
    .height_def = LV_SIZE_CONTENT,
    .editable = LV_OBJ_CLASS_EDITABLE_FALSE,
    .group_def = LV_OBJ_CLASS_GROUP_DEF_FALSE,
    .instance_size = sizeof(lv_obj_t),
};

static lv_obj_t* group_create(lv_obj_t* parent)
{
  return etx_create(&group_class, parent);
}

MixGroupBase::MixGroupBase(Window* parent, mixsrc_t idx) :
    Window(parent, rect_t{}, group_create), idx(idx)
{
  setWindowFlag(NO_FOCUS | NO_CLICK);

  label = etx_label_create(lvobj);
  etx_font(label, FONT_XS_INDEX, LV_STATE_USER_1);
}

void MixGroupBase::_adjustHeight(coord_t y)
{
  if (getLineCount() == 0) setHeight(ListLineButton::BTN_H + PAD_SMALL * 2);

  for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
    auto line = *it;
    line->updatePos(MixButtonBase::LN_X, y);
    y += line->height() + PAD_OUTLINE;
  }
  setHeight(y + PAD_BORDER * 2 + PAD_OUTLINE);
}

void MixGroupBase::adjustHeight()
{
  _adjustHeight(0);
}

void MixGroupBase::addLine(MixButtonBase* line)
{
  auto l = std::find_if(lines.begin(), lines.end(),
                        [=](const MixButtonBase* l) -> bool {
                          return line->getIndex() <= l->getIndex();
                        });

  if (l != lines.end())
    lines.insert(l, line);
  else
    lines.emplace_back(line);

  adjustHeight();
}

bool MixGroupBase::removeLine(MixButtonBase* line)
{
  auto l = std::find_if(
      lines.begin(), lines.end(),
      [=](const MixButtonBase* l) -> bool { return l == line; });

  if (l != lines.end()) {
    lines.erase(l);
    adjustHeight();
    return true;
  }

  return false;
}

void MixGroupBase::refresh()
{
  char* s = getSourceString(idx);
  if (getTextWidth(s, 0, FONT(STD)) > MixButtonBase::LN_X - PAD_TINY)
    lv_obj_add_state(label, LV_STATE_USER_1);
  else
    lv_obj_clear_state(label, LV_STATE_USER_1);
  lv_label_set_text(label, s);
}

int MixGroupBase::getLineNumber(uint8_t index)
{
  int n = 0;
  auto l = std::find_if(lines.begin(), lines.end(), [&](MixButtonBase* l) {
    n += 1;
    return l->getIndex() == index;
  });

  if (l != lines.end()) return n;

  return -1;
}

MixGroupBase* MixPageBase::getGroupBySrc(mixsrc_t src)
{
  auto g = std::find_if(
      groups.begin(), groups.end(),
      [=](MixGroupBase* g) -> bool { return g->getMixSrc() == src; });

  if (g != groups.end()) return *g;

  return nullptr;
}

void MixPageBase::removeGroup(MixGroupBase* g)
{
  auto group = std::find_if(groups.begin(), groups.end(),
                            [=](MixGroupBase* lh) -> bool { return lh == g; });
  if (group != groups.end()) groups.erase(group);
}

MixButtonBase* MixPageBase::getLineByIndex(uint8_t index)
{
  auto l = std::find_if(lines.begin(), lines.end(), [=](MixButtonBase* l) {
    return l->getIndex() == index;
  });

  if (l != lines.end()) return *l;

  return nullptr;
}

void MixPageBase::removeLine(MixButtonBase* l)
{
  auto line = std::find_if(lines.begin(), lines.end(),
                           [=](MixButtonBase* lh) -> bool { return lh == l; });
  if (line == lines.end()) return;

  line = lines.erase(line);
  while (line != lines.end()) {
    (*line)->setIndex((*line)->getIndex() - 1);
    ++line;
  }
}

void MixPageBase::addLineButton(mixsrc_t src, uint8_t index)
{
  MixGroupBase* group_w = getGroupBySrc(src);
  if (!group_w) {
    group_w = createGroup(form, src);
    // insertion sort
    groups.emplace_back(group_w);
    auto g = groups.rbegin();
    if (g != groups.rend()) {
      auto g_prev = g;
      ++g_prev;
      while (g_prev != groups.rend()) {
        if ((*g_prev)->getMixSrc() < (*g)->getMixSrc()) break;
        lv_obj_swap((*g)->getLvObj(), (*g_prev)->getLvObj());
        std::swap(*g, *g_prev);
        ++g;
        ++g_prev;
      }
    }
  }

  // create new line button
  auto btn = createLineButton(group_w, index);
  lv_group_focus_obj(btn->getLvObj());

  // insertion sort for the focus group
  auto l = lines.rbegin();
  if (l != lines.rend()) {
    auto l_prev = l;
    ++l_prev;
    while (l_prev != lines.rend()) {
      if ((*l_prev)->getIndex() < (*l)->getIndex()) break;
      (*l)->swapLvglGroup(*l_prev);
      std::swap(*l, *l_prev);
      // Inc index of elements after
      (*l)->setIndex((*l)->getIndex() + 1);
      ++l;
      ++l_prev;
    }
  }
}
