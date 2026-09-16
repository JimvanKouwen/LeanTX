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

#include "input_edit.h"

#include "curve_param.h"
#include "curveedit.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "getset_helpers.h"
#include "numberedit.h"
#include "input_source.h"
#include "textedit.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

#if LANDSCAPE
static const lv_coord_t col_dsc[] = {LV_GRID_FR(3), LV_GRID_FR(8),
                                     LV_GRID_TEMPLATE_LAST};
#else
static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2),
                                     LV_GRID_TEMPLATE_LAST};
#endif
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

InputEditWindow::InputEditWindow(int8_t input, uint8_t index) :
    Page(ICON_MODEL_INPUTS), input(input), index(index)
{
  header->setTitle(STR_MENUINPUTS);
  inputTitle = header->setTitle2("");

  setTitle();

#if PORTRAIT
  body->padAll(PAD_ZERO);

  auto box = new Window(body, rect_t{0, 0, body->width(), body->height() - INPUT_EDIT_CURVE_HEIGHT - PAD_TINY * 2});
  auto box_obj = box->getLvObj();
  etx_scrollbar(box_obj);
  box->padAll(PAD_SMALL);

  auto form = new Window(box, rect_t{});
  buildBody(form);

  preview = new Curve(
      body, rect_t{(LCD_W - INPUT_EDIT_CURVE_WIDTH) / 2, body->height() - INPUT_EDIT_CURVE_HEIGHT - PAD_TINY, INPUT_EDIT_CURVE_WIDTH, INPUT_EDIT_CURVE_HEIGHT},
      [=](int x) -> int {
        ExpoData* line = expoAddress(index);
        int16_t anas[MAX_INPUTS] = {0};
        applyExpos(anas, line->srcRaw, x);
        return anas[line->chn];
      },
      [=]() -> int { return getValue(expoAddress(index)->srcRaw); });
#else
  body->padAll(PAD_SMALL);
  buildBody(body);

  preview = new Curve(
      this, rect_t{LCD_W - INPUT_EDIT_CURVE_WIDTH - PAD_LARGE, EdgeTxStyles::MENU_HEADER_HEIGHT + PAD_TINY, INPUT_EDIT_CURVE_WIDTH, INPUT_EDIT_CURVE_HEIGHT},
      [=](int x) -> int {
        ExpoData* line = expoAddress(index);
        int16_t anas[MAX_INPUTS] = {0};
        applyExpos(anas, line->srcRaw, x);
        return anas[line->chn];
      },
      [=]() -> int { return getValue(expoAddress(index)->srcRaw); });
#endif
}

void InputEditWindow::setTitle()
{
  inputTitle->setText(getSourceString(MIXSRC_FIRST_INPUT + input));
}

void InputEditWindow::buildBody(Window* form)
{
  FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);
  form->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_ZERO);

  ExpoData* input = expoAddress(index);

  // Input Name
  auto line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_INPUTNAME);
  new ModelTextEdit(line, rect_t{}, g_model.inputNames[input->chn],
                    LEN_INPUT_NAME,
                    [=]() {
                      setTitle();
                    });

  // Line Name
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_EXPONAME);
  new ModelTextEdit(line, rect_t{}, input->name, LEN_EXPOMIX_NAME);

  // Source
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_SOURCE);
  auto src = new InputSource(line, input);
  lv_obj_set_style_grid_cell_x_align(src->getLvObj(), LV_GRID_ALIGN_STRETCH, 0);

  // Weight
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_WEIGHT);
  auto numberEdit =
      new NumberEdit(line, rect_t{}, -100, 100, GET_DEFAULT(input->weight),
                           [=](int32_t newValue) {
                             input->weight = newValue;
                             updatePreview = true;
                             SET_DIRTY();
                           });
  numberEdit->setSuffix("%");

  // Offset
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_OFFSET);
  numberEdit = new NumberEdit(line, rect_t{}, -100, 100,
                              GET_DEFAULT(input->offset), [=](int32_t newValue) {
                                input->offset = newValue;
                                updatePreview = true;
                                SET_DIRTY();
                              });
  numberEdit->setSuffix("%");

  // Curve
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_CURVE);
  auto param =
      new CurveParam(line, rect_t{}, &input->curve,
        [=](int32_t newValue) {
          input->curve.value = newValue;
          updatePreview = true;
          SET_DIRTY();
        }, input->srcRaw);
  lv_obj_set_style_grid_cell_x_align(param->getLvObj(), LV_GRID_ALIGN_STRETCH,
                                     0);

}

void InputEditWindow::checkEvents()
{
  if (_deleted) return;

  if (updatePreview) {
    updatePreview = false;
    Messaging::send(Messaging::CURVE_UPDATE);
  }

  Page::checkEvents();
}
