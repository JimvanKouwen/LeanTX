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

#include "module_setup.h"
#include "button.h"
#include "channel_range.h"
#include "choice.h"
#include "crossfire_settings.h"
#include "ext_antenna_settings.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "form.h"
#include "numberedit.h"
#include "getset_helpers.h"
#include "storage/modelslist.h"
#include "telemetry/crossfire.h"

#define SET_DIRTY() storageDirty(EE_MODEL)
#define ETX_STATE_UNIQUE_ID_WARN LV_STATE_USER_1

static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

class ModuleWindow : public Window
{
 public:
  ModuleWindow(Window* parent, uint8_t moduleIdx) :
      Window(parent, rect_t{}), moduleIdx(moduleIdx)
  {
    setFlexLayout();
    updateModule();
  }

  void updateModule()
  {
    clear();
    bindButton = nullptr;
    idUnique = nullptr;
    if (!isModuleCrossfire(moduleIdx)) return;

    FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);
    new CrossfireSettings(this, grid, moduleIdx);
#if defined(EXTERNAL_ANTENNA)
    if (moduleIdx == INTERNAL_MODULE &&
        g_eeGeneral.antennaMode == ANTENNA_MODE_PER_MODEL)
      new ExtAntennaSettings(this, grid, moduleIdx);
#endif

    auto line = newLine(grid);
    new StaticText(line, rect_t{}, STR_CHANNELRANGE);
    new ModuleChannelRange(line, moduleIdx);

    line = newLine(grid);
    new StaticText(line, rect_t{}, "");
    idUnique = new StaticText(line, rect_t{}, "");
    etx_txt_color(idUnique->getLvObj(), COLOR_THEME_WARNING_INDEX, ETX_STATE_UNIQUE_ID_WARN);
    updateIDStaticText(moduleIdx);

    line = newLine(grid);
    new StaticText(line, rect_t{}, STR_RECEIVER);
    auto box = new Window(line, rect_t{});
    box->padAll(PAD_TINY);
    box->setFlexLayout(LV_FLEX_FLOW_ROW, PAD_MEDIUM, LV_SIZE_CONTENT);
    auto modelId = &g_model.header.modelId[moduleIdx];
    new NumberEdit(box, {0, 0, EdgeTxStyles::EDIT_FLD_WIDTH_NARROW, 0}, 0, MAX_RXNUM,
                    GET_DEFAULT(*modelId), [=](int32_t value) {
                      *modelId = value;
                      modelslist.updateCurrentModelCell();
                      updateIDStaticText(moduleIdx);
                      RfService::requestModelId(moduleIdx);
                      SET_DIRTY();
                    });
    bindButton = new TextButton(box, rect_t{}, STR_MODULE_BIND, [=]() -> uint8_t {
      if (isModuleBindAvailable(moduleIdx)) {
        RfService::setMode(moduleIdx, MODULE_MODE_BIND);
        AUDIO_PLAY(AU_SPECIAL_SOUND_CHEEP);
      }
      return 0;
    });
    bindButton->show(isModuleBindAvailable(moduleIdx));
  }

 protected:
  uint8_t moduleIdx;
  TextButton* bindButton = nullptr;
  StaticText* idUnique = nullptr;

  void updateIDStaticText(int mdIdx)
  {
    if (idUnique == nullptr) return;
    char buffer[50];
    std::string idStr = STR_MODELIDUNIQUE;
    if (!modelslist.isModelIdUnique(mdIdx, buffer, sizeof(buffer))) {
      idStr = STR_MODELIDUSED;
      idStr = idStr + buffer;
      lv_obj_add_state(idUnique->getLvObj(), ETX_STATE_UNIQUE_ID_WARN);
    } else {
      lv_obj_clear_state(idUnique->getLvObj(), ETX_STATE_UNIQUE_ID_WARN);
    }
    idUnique->setText(idStr);
  }

  void checkEvents() override
  {
    if (bindButton != nullptr) {
      if (TELEMETRY_STREAMING() && isModuleELRS(moduleIdx))
        bindButton->setText(STR_MODULE_UNBIND);
      else if (isModuleELRS(moduleIdx))
        bindButton->setText(STR_MODULE_BIND);

      bindButton->show(isModuleBindAvailable(moduleIdx));
    }
    Window::checkEvents();
  }

};

ModulePage::ModulePage(uint8_t moduleIdx) : Page(ICON_MODEL_SETUP)
{
  header->setTitle(STR_MAIN_MODEL_SETTINGS);
  header->setTitle2(moduleIdx == INTERNAL_MODULE ? STR_INTERNALRF : STR_EXTERNALRF);
  body->setFlexLayout();
  FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);
  auto line = body->newLine(grid);
  new StaticText(line, rect_t{}, STR_MODE);
  auto md = &g_model.moduleData[moduleIdx];
  auto moduleChoice = new Choice(line, rect_t{}, STR_MODULE_PROTOCOLS,
                                 MODULE_TYPE_NONE, MODULE_TYPE_MAX, GET_DEFAULT(md->type));
  moduleChoice->setAvailableHandler([=](int8_t type) {
    return moduleIdx == INTERNAL_MODULE ? isInternalModuleAvailable(type)
                                        : isExternalModuleAvailable(type);
  });
  auto moduleWindow = new ModuleWindow(body, moduleIdx);
  moduleChoice->setSetValueHandler([=](int32_t type) {
    setModuleType(moduleIdx, type);
    moduleWindow->updateModule();
    SET_DIRTY();
  });
}
