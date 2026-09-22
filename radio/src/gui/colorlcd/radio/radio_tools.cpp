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

#include "radio_tools.h"

#include <algorithm>

#include "hal/module_port.h"
#include "lua/lua_api.h"
#include "edgetx.h"
#include "radio_gps_tool.h"
#include "radio_mic_recorder.h"
#include "standalone_lua.h"
#include "etx_lv_theme.h"
#include "lib_file.h"

//-----------------------------------------------------------------------------

inline bool tool_compare_nocase(const ToolEntry& first, const ToolEntry& second)
{
  return strcasecmp(first.label.c_str(), second.label.c_str()) < 0;
}

std::vector<ToolEntry> luaTools;
static bool luaToolsLoaded = false;

static void run_lua_tool(const std::string& path)
{
  char toolPath[FF_MAX_LFN + 1];
  strncpy(toolPath, path.c_str(), sizeof(toolPath) - 1);
  *((char*)getBasename(toolPath) - 1) = '\0';
  f_chdir(toolPath);

  LuaRuntime::executeStandalone(path.c_str());
}

void unloadLuaTools()
{
  luaTools.clear();
  luaToolsLoaded = false;
}

// LUA scripts in TOOLS
void loadLuaTools()
{
  if (!luaToolsLoaded) {
    luaToolsLoaded = true;

    FILINFO fno;
    DIR dir;

    FRESULT res = f_opendir(&dir, SCRIPTS_TOOLS_PATH);
    if (res == FR_OK) {
      for (;;) {
        res = f_readdir(&dir, &fno); /* Read a directory item */
        if (res != FR_OK || fno.fname[0] == 0)
          break; /* Break on error or end of dir */
        if (fno.fattrib & (AM_HID | AM_SYS))
          continue;  // skip hidden files and system files
        if (fno.fname[0] == '.') continue; /* Ignore UNIX hidden files */

        bool inFolder = (fno.fattrib & AM_DIR);

        char path[FF_MAX_LFN + 1] = SCRIPTS_TOOLS_PATH "/";
        strcat(path, fno.fname);
        if (inFolder) {
          // check if .lua with same name exists - skip folder to avoid duplicate entries
          auto plen = strlen(path);
          strcat(path, ".lua");
          if (f_stat(path, nullptr) == FR_OK)
            continue;
          path[plen] = 0;
          strcat(path, "/main.lua");
          if (f_stat(path, nullptr) != FR_OK)
            continue;
        }

        if (isRadioScriptTool(path)) {
          char toolName[RADIO_TOOL_NAME_MAXLEN + 1] = {0};
          const char* label;
          if (readToolName(toolName, path)) {
            label = toolName;
          } else {
            if (!inFolder) {
              char* ext = (char*)getFileExtension(fno.fname);
              if (ext) *ext = '\0';
            }
            label = fno.fname;
          }
          luaTools.emplace_back(ToolEntry{label, path, run_lua_tool});
        }
      }
    }

  std::sort(luaTools.begin(), luaTools.end(), tool_compare_nocase);
  }
}

// LUA scripts in TOOLS
static void scanLuaTools(std::list<ToolEntry>& scripts)
{
  loadLuaTools();

  for (auto t : luaTools)
    scripts.emplace_back(t);
}

const ToolEntry* getLuaTool(int n)
{
  if (n >= 0 && n < (int)luaTools.size())
    return &luaTools[n];
  return nullptr;
}

int getLuaToolId(const std::string name)
{
  int id = 0;
  for (auto t : luaTools) {
    if (t.label == name)
      return id;
    id += 1;
  }
  return -1;
}

void runLuaTool(const std::string name)
{
  for (auto t : luaTools) {
    if (t.label == name) {
      t.exec(t.path);
      return;
    }
  }
}

void getLuaToolNames(std::vector<std::string>& nameList)
{
  loadLuaTools();
  std::string s(STR_QM_APPS);
  s += " - ";
  for (size_t i = 0; i < luaTools.size(); i += 1) {
    nameList.emplace_back(s + luaTools[i].label);
  }
}
//-----------------------------------------------------------------------------

static bool isModelGPSSensorPresent()
{
  for (int i = 0; i < MAX_TELEMETRY_SENSORS; i++) {
    if (isGPSSensor(i+1)) return true;
  }
  return false;
}

static void run_gpstool(const std::string&)
{
  new RadioGpsTool();
}

#if defined(PDM_CLOCK)
static void run_mic_recorder(const std::string&)
{
  new RadioMicRecorder();
}
#endif

//-----------------------------------------------------------------------------

struct ToolButton : public TextButton {
  ToolButton(Window* parent, const ToolEntry& tool) :
      TextButton(parent, rect_t{}, tool.label, [=]() {
        tool.exec(tool.path);
        return 0;
      })
  {
    setWidth(TOOLS_BTN_W);
    setHeight(TOOLS_BTN_H);

    lv_obj_set_width(label, lv_pct(100));
    etx_obj_add_style(label, styles->text_align_center, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  }

  static LAYOUT_ORIENTATION(TOOLS_BTN_W, (LCD_W - PAD_LARGE * 3) / 3, (LCD_W - PAD_LARGE * 2) / 2)
  static LAYOUT_VAL_SCALED(TOOLS_BTN_H, 48)
};

//-----------------------------------------------------------------------------

RadioToolsPage::RadioToolsPage(const PageDef& pageDef) : PageGroupItem(pageDef) {}

void RadioToolsPage::build(Window* window)
{
  this->window = window;

  memclear(&reusableBuffer.radioTools, sizeof(reusableBuffer.radioTools));
  waiting = 0;

  rebuild(window);
}

void RadioToolsPage::checkEvents()
{

  PageGroupItem::checkEvents();
}

void RadioToolsPage::rebuild(Window* window)
{
  window->clear();

  std::list<ToolEntry> tools;


  if (isModelGPSSensorPresent())
    tools.emplace_back(ToolEntry{STR_GPS_MODEL_LOCATOR, "", run_gpstool});



#if defined(PDM_CLOCK)
  tools.emplace_back(ToolEntry{STR_MIC_RECORDER, "", run_mic_recorder});
#endif

  scanLuaTools(tools);

  tools.sort(tool_compare_nocase);

  window->setFlexLayout(LV_FLEX_FLOW_ROW_WRAP, PAD_MEDIUM);

  for (const auto& tool : tools) {
    new ToolButton(window, tool);
  }
}
