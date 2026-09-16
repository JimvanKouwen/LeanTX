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

#include <vector>
#include <string>

#include "etx_lv_theme.h"

struct WidgetOption;

//-----------------------------------------------------------------------------

enum WidgetOptionAlign
{
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT,

  // this one MUST be last
  ALIGN_COUNT
};

enum WidgetOptionValueEnum {
  WOV_Unsigned=0,
  WOV_Signed,
  WOV_Bool,
  WOV_String,
  WOV_Source,
  WOV_Color
};

#define WIDGET_OPTION_VALUE_SIGNED(x)   WidgetOptionValue{ .signedValue = (x) }
#define WIDGET_OPTION_VALUE_STRING(...) WidgetOptionValue{ .stringValue = { __VA_ARGS__ } }

struct WidgetOptionValue
{
  union {
    uint32_t unsignedValue;
    int32_t signedValue;
    uint32_t boolValue;
  };
  std::string stringValue;
};

struct WidgetOptionValueTyped
{
  WidgetOptionValueEnum type;
  WidgetOptionValue value;
};

//-----------------------------------------------------------------------------

#define MAX_WIDGET_OPTIONS 50   // For YAML parser

struct WidgetPersistentData {
  std::vector<WidgetOptionValueTyped> options;
  void addEntry(int idx);
  bool hasOption(int idx);
  void setDefault(int idx, const WidgetOption* opt, bool forced);
  void clear();
  WidgetOptionValueEnum getType(int idx);
  void setType(int idx, WidgetOptionValueEnum typ);
  int32_t getSignedValue(int idx);
  void setSignedValue(int idx, int32_t newValue);
  uint32_t getUnsignedValue(int idx);
  void setUnsignedValue(int idx, uint32_t newValue);
  bool getBoolValue(int idx);
  void setBoolValue(int idx, bool newValue);
  std::string getString(int idx);
  void setString(int idx, const char* s);
};

//-----------------------------------------------------------------------------

enum LayoutOptionValueEnum {
  LOV_None=0,
  LOV_Bool,
  LOV_Color
};

union LayoutOptionValue
{
  uint32_t unsignedValue;
  uint32_t boolValue;
};

struct LayoutOptionValueTyped
{
  LayoutOptionValueEnum type;
  LayoutOptionValue value;
};

#define MAX_LAYOUT_ZONES 10
#define MAX_LAYOUT_OPTIONS 10

struct ZonePersistentData {
  std::string widgetName;
  WidgetPersistentData widgetData;
  void clear();
};

struct LayoutPersistentData {
  ZonePersistentData zones[MAX_LAYOUT_ZONES];
  LayoutOptionValueTyped options[MAX_LAYOUT_OPTIONS];
  void clearZone(int idx);
  void clear();
  const char* getWidgetName(int idx);
  void setWidgetName(int idx, const char* s);
  WidgetPersistentData* getWidgetData(int idx);
  bool hasWidget(int idx);
};

//-----------------------------------------------------------------------------

struct CustomScreenData {
  std::string LayoutId;
  LayoutPersistentData layoutData;
};

//-----------------------------------------------------------------------------

static LAYOUT_VAL_SCALED(MENU_HEADER_BUTTONS_LEFT, 47)

#if WIDE_LAYOUT
static LAYOUT_VAL_SCALED(TOPBAR_ZONE_WIDTH, 74)
#else
static LAYOUT_VAL_SCALED(TOPBAR_ZONE_WIDTH, 70)
#endif
static constexpr int MAX_TOPBAR_ZONES = (LCD_W - MENU_HEADER_BUTTONS_LEFT - 1 + TOPBAR_ZONE_WIDTH / 2) / TOPBAR_ZONE_WIDTH;

struct TopBarPersistentData {
  ZonePersistentData zones[MAX_TOPBAR_ZONES];
  void clearZone(int idx);
  void clear();
  const char* getWidgetName(int idx);
  void setWidgetName(int idx, const char* s);
  WidgetPersistentData* getWidgetData(int idx);
  bool hasWidget(int idx);
  bool isWidget(int idx, const char* s);
};

//-----------------------------------------------------------------------------
