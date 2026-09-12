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

#include "radio_version.h"

#include "button.h"
#include "dialog.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "fw_version.h"
#include "hal/module_port.h"
#include "options.h"
#include "static.h"

#if defined(CROSSFIRE)
#include "mixer_scheduler.h"
#endif

static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2),
                                     LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

class VersionDialog : public BaseDialog
{
  Window* int_module_name_w;
  StaticText* int_name;
  Window* int_module_status_w;
  StaticText* int_status;

  Window* int_rx_name_w;
  StaticText* int_rx_name;
  Window* int_rx_status_w;
  StaticText* int_rx_status;

  Window* ext_module_name_w;
  StaticText* ext_name;
  Window* ext_module_status_w;
  StaticText* ext_status;

  Window* ext_rx_name_w;
  StaticText* ext_rx_name;
  Window* ext_rx_status_w;
  StaticText* ext_rx_status;

 public:
  VersionDialog() :
      BaseDialog(STR_MODULES_RX_VERSION, true)
  {

    // define grid layout
    FlexGridLayout grid(col_dsc, row_dsc);

    auto g = lv_group_get_default();
    lv_group_set_editing(g, true);

    lv_group_add_obj(g, form->getLvObj());

    // headline "Internal module"
    new StaticText(form, rect_t{}, STR_INTERNAL_MODULE);

    // Internal module name
    int_module_name_w = form->newLine(grid);
    new StaticText(int_module_name_w, rect_t{}, STR_MODULE);
    int_name = new StaticText(int_module_name_w, rect_t{}, "");

    // internal module status
    int_module_status_w = form->newLine(grid);
    new StaticText(int_module_status_w, rect_t{}, STR_STATUS);
    int_status = new StaticText(int_module_status_w, rect_t{}, "");
    int_module_status_w->hide();

    // internal receiver name
    int_rx_name_w = form->newLine(grid);
    new StaticText(int_rx_name_w, rect_t{}, STR_RECEIVER);
    int_rx_name =
        new StaticText(int_rx_name_w, rect_t{}, "");
    int_rx_name_w->hide();

    // internal receiver status
    int_rx_status_w = form->newLine(grid);
    new StaticText(int_rx_status_w, rect_t{}, STR_STATUS);
    int_rx_status =
        new StaticText(int_rx_status_w, rect_t{}, "");
    int_rx_status_w->hide();

    // headline "External module"
    new StaticText(form, rect_t{}, STR_EXTERNAL_MODULE);

    // external module name
    ext_module_name_w = form->newLine(grid);
    new StaticText(ext_module_name_w, rect_t{}, STR_MODULE);
    ext_name = new StaticText(ext_module_name_w, rect_t{}, "");

    // external module status
    ext_module_status_w = form->newLine(grid);
    new StaticText(ext_module_status_w, rect_t{}, STR_STATUS);
    ext_status = new StaticText(ext_module_status_w, rect_t{}, "");
    ext_module_status_w->hide();

    // external receiver name
    ext_rx_name_w = form->newLine(grid);
    new StaticText(ext_rx_name_w, rect_t{}, STR_RECEIVER);
    ext_rx_name =
        new StaticText(ext_rx_name_w, rect_t{}, "");
    ext_rx_name_w->hide();

    // external receiver status
    ext_rx_status_w = form->newLine(grid);
    new StaticText(ext_rx_status_w, rect_t{}, STR_STATUS);
    ext_rx_status =
        new StaticText(ext_rx_status_w, rect_t{}, "");
    ext_rx_status_w->hide();

    // content->setWidth(LCD_W * 0.8);
    update();
  }

  void update()
  {
#if defined(HARDWARE_INTERNAL_MODULE)
    updateModule(INTERNAL_MODULE, int_name, int_module_status_w, int_status,
                 int_rx_name_w, int_rx_name, int_rx_status_w, int_rx_status);
#endif
    updateModule(EXTERNAL_MODULE, ext_name, ext_module_status_w, ext_status,
                 ext_rx_name_w, ext_rx_name, ext_rx_status_w, ext_rx_status);
  }

  void updateModule(uint8_t module, StaticText* name, Window* module_status_w,
                    StaticText* status, Window* rx_name_w, StaticText* rx_name,
                    Window* rx_status_w, StaticText* rx_status)
  {
    // initialize module name with module selection made in model settings
    // initialize to module does not provide status
    // CRSF will overwrite status
    name->setText(STR_MODULE_PROTOCOLS[g_model.moduleData[module].type]);
    module_status_w->hide();

#if defined(CROSSFIRE)
    // CRSF is able to provide status
    if (isModuleCrossfire(module)) {
      char statusText[64];

      auto hz = 1000000 / getMixerSchedulerPeriod();
      // snprintf(statusText, 64, "%d Hz %" PRIu32 " Err", hz, telemetryErrors);
      snprintf(statusText, 64, "%d Hz", hz);
      status->setText(statusText);
      snprintf(statusText, 64, "%s V%u.%u.%u", crossfireModuleStatus[module].name, crossfireModuleStatus[module].major, crossfireModuleStatus[module].minor, crossfireModuleStatus[module].revision);
      name->setText(statusText);
      module_status_w->show();
    }
#endif

  }

};

#if VERSION_MAJOR == 2 && LCD_H == 272
const std::string copyright_str = "(C) " BUILD_YEAR " EdgeTX";
#else
const std::string copyright_str = "Copyright (C) " BUILD_YEAR " EdgeTX";
#endif
const std::string edgetx_url = "https://edgetx.org";

RadioVersionPage::RadioVersionPage(const PageDef& pageDef) :
    PageGroupItem(pageDef)
{
}

#if defined(PCBPL18)
extern const char* boardLcdType;
extern const char* boardTouchType;
#endif

void RadioVersionPage::build(Window* window)
{
  window->padAll(PAD_ZERO);

  coord_t qw, qh, iw, ih, ix, iy;

#if LANDSCAPE
  qw = QR_SZ + PAD_LARGE * 2;
  qh = window->height();
  iw = window->width() - qw;
  ih = qh;
  ix = qw;
  iy = 0;
#else
  qw = window->width();
  qh = QR_SZ + EdgeTxStyles::STD_FONT_HEIGHT * 2 + PAD_LARGE + PAD_SMALL;
  iw = qw;
  ih = window->height() - qh;
  ix = 0;
  iy = qh;
#endif

  auto qrBox = new Window(window, {0, 0, qw, qh});
  qrBox->padAll(PAD_ZERO);

  new StaticText(qrBox, {0, PAD_SMALL, LV_PCT(100), 0}, copyright_str,
                 COLOR_THEME_SECONDARY1_INDEX, CENTERED);

  new StaticText(qrBox, {0, qh - QR_SZ - PAD_MEDIUM - EdgeTxStyles::STD_FONT_HEIGHT, LV_PCT(100), 0},
                 edgetx_url, COLOR_THEME_SECONDARY1_INDEX, CENTERED);

  new QRCode(qrBox, (qw - QR_SZ) / 2, qh - QR_SZ - PAD_MEDIUM, QR_SZ, edgetx_url);

  auto infoBox = new Window(window, {ix, iy, iw, ih});
  infoBox->padAll(PAD_SMALL);
  infoBox->padLeft(PAD_LARGE);
  infoBox->padRight(PAD_LARGE);

  std::string nl("\n");
  std::string version;

  version += fw_stamp + nl;
  version += vers_stamp + nl;
  version += date_stamp + nl;
  version += time_stamp + nl;
  version += "OPTS: ";

  for (uint8_t i = 0; options[i]; i++) {
    if (i > 0) version += ", ";
    version += options[i];
  }

#if defined(RADIO_T15)
  version += nl;
  version += "PCBREV: ";
  version += '0' + hardwareOptions.pcbrev;
#endif

#if defined(PCBPL18) && !defined(SIMU)
  version += nl;
  version += "LCD: ";
  version += boardLcdType;
  version += nl;
  version += "Touch: ";
  version += boardTouchType;
#endif

  new StaticText(infoBox, {0, 0, LV_PCT(100), LV_SIZE_CONTENT}, version);

  // Module and receivers versions
  new TextButton(infoBox, {0, ih - EdgeTxStyles::UI_ELEMENT_HEIGHT - PAD_LARGE - PAD_SMALL, LV_PCT(100), 0},
                  STR_MODULES_RX_VERSION, [=]() {
                    new VersionDialog();
                    return 0;
                  });
}
