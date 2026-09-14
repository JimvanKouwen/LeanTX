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

#include "model_gvars.h"

#include "choice.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "getset_helpers.h"
#include "list_line_button.h"
#include "menu.h"
#include "numberedit.h"
#include "page.h"
#include "textedit.h"
#include "toggleswitch.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

class GVarButton : public ListLineButton
{
 public:
  GVarButton(Window* parent, uint8_t gvar) : ListLineButton(parent, gvar)
  {
    padLeft(PAD_LARGE);
    setHeight(BTN_H);
    delayLoad();
  }
  static LAYOUT_SIZE_SCALED(BTN_H, 32, 50)

 protected:
  lv_obj_t* valueText = nullptr;
  gvar_t lastValue = 0;

  void delayedInit() override
  {
    auto name = etx_label_create(lvobj);
    lv_label_set_text(name, getGVarString(index));
    lv_obj_set_pos(name, PAD_TINY, PAD_TINY);
    valueText = etx_label_create(lvobj);
    lv_obj_set_pos(valueText, LAYOUT_SCALE(60), PAD_TINY);
    updateValue();
  }
  void updateValue()
  {
    lastValue = getGVarValue(index);
    lv_label_set_text(valueText, formatGVarValue(index, lastValue, 0).c_str());
  }
  void checkEvents() override
  {
    ListLineButton::checkEvents();
    if (loaded && lastValue != getGVarValue(index)) updateValue();
  }
  bool isActive() const override { return false; }
  void refresh() override {}
};

class GVarEditWindow : public Page
{
 public:
  explicit GVarEditWindow(uint8_t gvarIndex) :
      Page(ICON_MODEL_GVARS), index(gvarIndex)
  {
    buildHeader(header);
    buildBody(body);
  }

 protected:
  uint8_t index;
  gvar_t lastGVar = 0;
  bool refreshTitle = true;
  NumberEdit* min = nullptr;
  NumberEdit* max = nullptr;
  NumberEdit* value = nullptr;
  StaticText* gVarInHeader = nullptr;

  void buildHeader(Window* window)
  {
    header->setTitle(STR_MENU_GLOBAL_VARS);
    gVarInHeader = header->setTitle2("");
  }

  void checkEvents()
  {

    Page::checkEvents();
    if (gVarInHeader && (lastGVar != getGVarValue(index) || refreshTitle)) {
      refreshTitle = false;
      lastGVar = getGVarValue(index);
      std::string title = getSourceString(index + MIXSRC_FIRST_GVAR);
      title += "=";
      title += formatGVarValue(index, lastGVar, 0);
      gVarInHeader->setText(title.c_str());
    }

}

  void setProperties()
  {
    GVarData* gvar = &g_model.gvars[index];
    int32_t minValue = GVAR_MIN + gvar->min;
    int32_t maxValue = GVAR_MAX - gvar->max;
    const char* suffix = gvar->unit ? "%" : "";

    if (min && max) {
      min->setMax(maxValue);
      max->setMin(minValue);

      min->setSuffix(suffix);
      max->setSuffix(suffix);

      if (gvar->prec) {
        min->setTextFlag(PREC1);
        max->setTextFlag(PREC1);
      } else {
        min->clearTextFlag(PREC1);
        max->clearTextFlag(PREC1);
      }

      min->update();
      max->update();
    }
    if (value) {
      value->setMin(minValue);
      value->setMax(maxValue);
      value->setValue(value->getValue());
      value->setSuffix(suffix);
      if (gvar->prec) value->setTextFlag(PREC1);
      else value->clearTextFlag(PREC1);
      value->update();
    }
  }

  void buildBody(Window* window)
  {
    static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                         LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_CONTENT,
                                         LV_GRID_TEMPLATE_LAST};

    window->setFlexLayout();
    FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);

    auto line = window->newLine(grid);

    GVarData* gvar = &g_model.gvars[index];

    new StaticText(line, rect_t{}, STR_NAME);
    grid.nextCell();
    new ModelTextEdit(line, rect_t{}, gvar->name, LEN_GVAR_NAME, [=]() { refreshTitle = true; });

    line = window->newLine(grid);

    static const char* const strUnits[] = { "-", "%" };
    new StaticText(line, rect_t{}, STR_UNIT);
    grid.nextCell();
    new Choice(line, rect_t{}, strUnits, 0, 1, GET_DEFAULT(gvar->unit),
               [=](int16_t newValue) {
                 refreshTitle = (gvar->unit != newValue);
                 gvar->unit = newValue;
                 SET_DIRTY();
                 setProperties();
               });

    line = window->newLine(grid);

    new StaticText(line, rect_t{}, STR_PRECISION);
    grid.nextCell();
    new Choice(line, rect_t{}, STR_VPREC, 0, 1, GET_DEFAULT(gvar->prec),
               [=](int16_t newValue) {
                 refreshTitle = (gvar->prec != newValue);
                 gvar->prec = newValue;
                 SET_DIRTY();
                 setProperties();
               });

    line = window->newLine(grid);

    new StaticText(line, rect_t{}, STR_MIN);
    grid.nextCell();
    min = new NumberEdit(
        line, rect_t{}, GVAR_MIN, GVAR_MAX - gvar->max,
        [=] { return gvar->min + GVAR_MIN; },
        [=](int32_t newValue) {
          gvar->min = newValue - GVAR_MIN;
          SET_DIRTY();
          setProperties();
        });
    min->setAccelFactor(16);

    line = window->newLine(grid);

    new StaticText(line, rect_t{}, STR_MAX);
    grid.nextCell();
    max = new NumberEdit(
        line, rect_t{}, GVAR_MIN + gvar->min, GVAR_MAX,
        [=] { return GVAR_MAX - gvar->max; },
        [=](int32_t newValue) {
          gvar->max = GVAR_MAX - newValue;
          SET_DIRTY();
          setProperties();
        });
    max->setAccelFactor(16);

    line = window->newLine(grid);
    new StaticText(line, rect_t{}, STR_POPUP);
    grid.nextCell();
    new ToggleSwitch(line, rect_t{}, GET_SET_DEFAULT(gvar->popup));

    line = window->newLine(grid);
    new StaticText(line, rect_t{}, STR_VALUE);
    grid.nextCell();
    value = new NumberEdit(
        line, rect_t{}, GVAR_MIN + gvar->min, GVAR_MAX - gvar->max,
        [=]() { return getGVarValue(index); },
        [=](int32_t newValue) { setGVarValue(index, newValue); });
    value->setAccelFactor(16);

    setProperties();
    lv_obj_set_height(window->getLvObj(),
                      LCD_H - lv_obj_get_height(header->getLvObj()));
    lv_obj_set_height(lvobj, LCD_H);
  }
};

ModelGVarsPage::ModelGVarsPage(const PageDef& pageDef) :
    PageGroupItem(pageDef)
{
}

void ModelGVarsPage::rebuild(Window* window)
{
  auto scroll_y = lv_obj_get_scroll_y(window->getLvObj());
  window->clear();
  build(window);
  lv_obj_scroll_to_y(window->getLvObj(), scroll_y, LV_ANIM_OFF);
}

void ModelGVarsPage::build(Window* window)
{
  for (uint8_t index = 0; index < MAX_GVARS; index++) {
    auto button = new GVarButton(window, index);
    lv_obj_set_pos(button->getLvObj(), 0, index * (GVarButton::BTN_H + PAD_OUTLINE));
    button->setPressHandler([=]() {
      Menu* menu = new Menu();
      menu->addLine(STR_EDIT, [=]() {
        Window* editWindow = new GVarEditWindow(index);
        editWindow->setCloseHandler([=]() { rebuild(window); });
      });
      menu->addLine(STR_CLEAR, [=]() { setGVarValue(index, 0); });
      return 0;
    });
  }
}
