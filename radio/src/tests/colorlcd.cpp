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

#include "gtests.h"

#if defined(COLORLCD)

#include "colors.h"

TEST(color, RGB)
{
  // test conversion from RGB to RGB565 (5 bits for R, 6 bits for G, 5 bits for B)
  EXPECT_EQ(RGB(0, 0, 0), (uint16_t)0x0000);
  EXPECT_EQ(RGB(255, 255, 255), (uint16_t)0xFFFF);

  EXPECT_EQ(RGB(255, 0, 0), (uint16_t)0xF800);
  EXPECT_EQ(RGB(0, 255, 0), (uint16_t)0x07E0);
  EXPECT_EQ(RGB(0, 0, 255), (uint16_t)0x001F);

  EXPECT_EQ(RGB(30, 40, 150), (uint16_t)0x1952);
}

TEST(color, ARGB)
{
  // test conversion from RGB to ARGB4444 (4 bits for alpha, 4 bits for R, 4 bits for G, 4 bits for B)
  EXPECT_EQ(ARGB(0, 0, 0, 0), (uint16_t)0x0000);
  EXPECT_EQ(ARGB(255, 255, 255, 255), (uint16_t)0xFFFF);

  EXPECT_EQ(ARGB(255, 0, 0, 0), (uint16_t)0xF000);
  EXPECT_EQ(ARGB(0, 255, 0, 0), (uint16_t)0x0F00);
  EXPECT_EQ(ARGB(0, 0, 255, 0), (uint16_t)0x00F0);
  EXPECT_EQ(ARGB(0, 0, 0, 255), (uint16_t)0x000F);

  EXPECT_EQ(ARGB(128, 30, 40, 150), (uint16_t)0x8129);
}

#endif

#if defined(COLORLCD)
#include "widget.h"

TEST(ValueWidget, InvalidSourceAndGpsRefresh)
{
  g_model.resetScreenData();
  g_model.setScreenLayoutId(0, "Layout1x1");
  auto factory = WidgetFactory::getWidgetFactory("Value");
  ASSERT_NE(nullptr, factory);
  auto widget = factory->create(nullptr, {0, 0, 200, 100}, 0, 0);
  ASSERT_NE(nullptr, widget);
  lv_event_send(widget->getLvObj(), LV_EVENT_DRAW_MAIN_BEGIN, nullptr);
  auto& source = widget->getPersistentData()->options[0].value.unsignedValue;
  auto text = [&]() {
    return std::string(lv_label_get_text(lv_obj_get_child(widget->getLvObj(), 3)));
  };
  for (unsigned id : {unsigned(MIXSRC_LAST_TELEM + 1), 32767u, 65536u, 0x80000000u}) {
    source = id;
    widget->update();
    widget->foreground();
    EXPECT_EQ("0", text());
  }
  g_vbat100mV = 87;
  source = uint32_t(-MIXSRC_TX_VOLTAGE);
  widget->update();
  widget->foreground();
  EXPECT_NE(std::string::npos, text().find("-8.7"));
#if defined(INTERNAL_GPS)
  auto savedGps = gpsData;
  source = MIXSRC_TX_GPS;
  gpsData.fix = false;
  gpsData.numSat = 2;
  widget->update();
  widget->foreground();
  EXPECT_NE(std::string::npos, text().find("2"));
  auto old = text();
  gpsData.numSat = 7;
  widget->foreground();
  EXPECT_NE(old, text());
  gpsData.fix = true;
  gpsData.latitude = 12345678;
  gpsData.longitude = 23456789;
  widget->foreground();
  old = text();
  gpsData.longitude += 1000000;
  widget->foreground();
  EXPECT_NE(old, text());
  gpsData.fix = false;
  widget->foreground();
  EXPECT_NE(std::string::npos, text().find("sats:"));
  gpsData = savedGps;
#endif
  delete widget;
  g_model.resetScreenData();
}
#endif
