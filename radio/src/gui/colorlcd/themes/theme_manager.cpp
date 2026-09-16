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
#include "theme_manager.h"

#include "../../storage/sdcard_common.h"
#include "theme_config.h"
#include "etx_lv_theme.h"
#include "hal/abnormal_reboot.h"
#include "lib_file.h"
#include "mainwindow.h"
#include "pagegroup.h"
#include "topbar.h"
#include "view_main.h"

#define SET_DIRTY() storageDirty(EE_GENERAL)

#define MAX_FILES 9

ThemePersistance ThemePersistance::themePersistance;

static const char* const colorNames[THEME_COLOR_COUNT] = {
    STR_THEME_COLOR_PRIMARY1,   STR_THEME_COLOR_PRIMARY2,
    STR_THEME_COLOR_PRIMARY3,   STR_THEME_COLOR_SECONDARY1,
    STR_THEME_COLOR_SECONDARY2, STR_THEME_COLOR_SECONDARY3,
    STR_THEME_COLOR_FOCUS,      STR_THEME_COLOR_EDIT,
    STR_THEME_COLOR_ACTIVE,     STR_THEME_COLOR_WARNING,
    STR_THEME_COLOR_DISABLED,   STR_THEME_COLOR_QM_BG,
    STR_THEME_COLOR_QM_FG,      STR_THEME_COLOR_CUSTOM,
};

ThemeFile::ThemeFile(std::string themePath, bool loadYAML) : path(themePath)
{
  if (loadYAML && path.size()) {
    deSerialize();
  }

  auto found = path.rfind('/');
  if (found != std::string::npos) {
    int n = 0;
    while (n < MAX_FILES) {
      auto baseFileName(path.substr(0, found + 1) +
                        (n != 0 ? "screenshot" + std::to_string(n) : "logo") +
                        ".png");
      if (isFileAvailable(baseFileName.c_str(), true)) {
        _imageFileNames.emplace_back(baseFileName);
      } else {
        break;
      }

      n++;
    }
  }
}

ThemeFile& ThemeFile::operator=(const ThemeFile& theme)
{
  if (this == &theme) return *this;

  path = theme.path;
  name = theme.name;
  author = theme.author;
  info = theme.info;
  colorList.assign(theme.colorList.begin(), theme.colorList.end());
  _imageFileNames.assign(theme._imageFileNames.begin(),
                         theme._imageFileNames.end());

  return *this;
};

std::vector<std::string> ThemeFile::getThemeImageFileNames()
{
  return _imageFileNames;
}

void ThemeFile::serialize()
{
  ThemeConfig yt;

  strAppend(yt.name, name.c_str(), SELECTED_THEME_NAME_LEN);
  strAppend(yt.author, author.c_str(), ThemeFile::AUTHOR_LENGTH);
  strAppend(yt.info, info.c_str(), ThemeFile::INFO_LENGTH);
  for (auto colorEntry : colorList) {
    if (colorEntry.colorNumber >= 0 && colorEntry.colorNumber < THEME_COLOR_COUNT - 1)
      yt.colors[colorEntry.colorNumber] = colorEntry.colorValue;
  }

  auto err = saveThemeConfig(path.c_str(), yt);
  if (err != nullptr) {
    ALERT(STR_WARNING, err, AU_WARNING1);
  }
}

void ThemeFile::deSerialize()
{
  ThemeConfig yt;

  // initialize the color table to defaults (in case of missing entries)
  for (uint8_t i = COLOR_THEME_PRIMARY1_INDEX; i < THEME_COLOR_COUNT - 1; i += 1)
    yt.colors[i] = defaultColors[i];

  auto err = loadThemeConfig(path.c_str(), yt);

  if (err == nullptr) {
    name = yt.name;
    author = yt.author;
    info = yt.info;
    for (int i = 0; i < THEME_COLOR_COUNT - 1; i += 1) {
      colorList.emplace_back(
          ColorEntry{(LcdColorIndex)(i), yt.colors[i]});
    }
  } else {
    ALERT(STR_WARNING, err, AU_WARNING1);
  }
}

void ThemeFile::setColor(LcdColorIndex colorIndex, uint32_t color)
{
  auto colorEntry =
      std::find(colorList.begin(), colorList.end(), ColorEntry{colorIndex, 0});
  if (colorEntry != colorList.end())
    colorEntry->colorValue = color;
  else
    colorList.emplace_back(ColorEntry{colorIndex, color});
}

void ThemeFile::applyColors()
{
  for (auto color : colorList) {
    lcdColorTable[color.colorNumber] = color.colorValue;
  }
}

bool ThemeFile::tryBackground(std::string& file)
{
  if (isFileAvailable(file.c_str()))
    return MainWindow::instance()->setBackgroundImage(file);
  return false;
}

void ThemeFile::applyBackground()
{
  std::string backgroundImageFileName(getPath());
  auto pos = backgroundImageFileName.rfind('/');
  if (pos != std::string::npos) {
    auto rootDir = backgroundImageFileName.substr(0, pos + 1);
    rootDir = rootDir + "background_" + std::to_string(LCD_W) + "x" +
              std::to_string(LCD_H) + ".png";

    if (tryBackground(rootDir))
      return;

    rootDir = backgroundImageFileName.substr(0, pos + 1);
    rootDir = rootDir + "background.png";

    if (tryBackground(rootDir))
      return;
  }

  // Use EdgeTxTheme default background
  std::string defaultBackground(THEMES_PATH "/EdgeTX/background.png");
  tryBackground(defaultBackground);
}

void ThemeFile::applyTheme()
{
  applyColors();
  applyBackground();
  styles->applyColors();
}

// avoid leaking memory
void ThemePersistance::clearThemes()
{
  for (auto theme : themes) {
    delete theme;
  }
  themes.clear();
}

void ThemePersistance::scanThemeFolder(char* themeFolder)
{
  char themePath[FF_MAX_LFN + 1];
  char* s = strAppend(themePath, THEMES_PATH "/", FF_MAX_LFN);
  s = strAppend(s, themeFolder, FF_MAX_LFN - (s - themePath));
  strAppend(s, "/theme.yml", FF_MAX_LFN - (s - themePath));
  if (isFileAvailable(themePath, true)) {
    TRACE("scanForThemes: found file %s", themePath);
    themes.emplace_back(new ThemeFile(themePath));
  }
}

void ThemePersistance::scanForThemes()
{
  clearThemes();

  DIR dir;
  FILINFO fno;

  char fullPath[FF_MAX_LFN + 1];

  strAppend(fullPath, THEMES_PATH, FF_MAX_LFN);

  TRACE("opening directory: %s", fullPath);
  FRESULT res = f_opendir(&dir, fullPath);  // Open the directory
  if (res == FR_OK) {
    TRACE("scanForThemes: open successful");
    // read all entries
    bool firstTime = true;
    for (;;) {
      res = sdReadDir(&dir, &fno, firstTime);

      if (res != FR_OK || fno.fname[0] == 0)
        break;  // Break on error or end of dir

      if (strlen((const char*)fno.fname) > SD_SCREEN_FILE_LENGTH) continue;
      if (fno.fattrib & AM_DIR)
        scanThemeFolder(fno.fname);
    }

    f_closedir(&dir);
    std::sort(themes.begin(), themes.end(), [](ThemeFile* a, ThemeFile* b) {
      return a->getName().compare(b->getName()) < 0;
    });
  }
}

void ThemePersistance::refresh()
{
  if (!UNEXPECTED_SHUTDOWN())
    scanForThemes();
  insertDefaultTheme();
}

void ThemePersistance::loadDefaultTheme()
{
  refresh();

  int index = 0;
  bool found = false;

  // Load theme from 'selectedtheme.txt' file, if not set in radio settings
  // TODO: remove this sometime in the future
  if (g_eeGeneral.selectedTheme[0] == 0) {
    constexpr const char* SELECTED_THEME_FILE =
        THEMES_PATH "/selectedtheme.txt";

    FIL file;
    FRESULT status = f_open(&file, SELECTED_THEME_FILE, FA_READ);

    if (status == FR_OK) {
      char line[256];
      unsigned int len;

      status = f_read(&file, line, 256, &len);
      if (status == FR_OK) {
        line[len] = '\0';

        for (auto theme : themes) {
          if (theme->getPath() == std::string(line)) {
            found = true;
            break;
          }
          index++;
        }

        // Force default if no match for last selected theme
        if (!found) index = 0;
      }

      f_close(&file);

      // Delete old file
      f_unlink(SELECTED_THEME_FILE);
    }

    // Save selected theme (sets to default if nothing found)
    setDefaultTheme(index);

    // Reset values (used below);
    index = 0;
    found = false;
  }

  for (auto theme : themes) {
    if (theme->getName().compare(0, SELECTED_THEME_NAME_LEN, g_eeGeneral.selectedTheme) == 0) {
      found = true;
      break;
    }
    index++;
  }

  // Force default if no match for last selected theme
  if (!found) index = 0;

  applyTheme(index);
  setThemeIndex(index);
}

char** ThemePersistance::getColorNames() { return (char**)colorNames; }

bool ThemePersistance::deleteThemeByIndex(int index)
{
  // greater than 0 is intentional here.  cant delete default theme.
  if (index > 0 && index < (int)themes.size()) {
    ThemeFile* theme = themes[index];

    char newFile[FF_MAX_LFN + 10];
    strAppend(newFile, theme->getPath().c_str(), FF_MAX_LFN);
    strcat(newFile, ".deleted");

    if (isFileAvailable(newFile, true)) {
      // .deleted file already exists, remove it
      f_unlink(newFile);
    }

    // for now we are just renaming the file so we don't find it
    FRESULT status = f_rename(theme->getPath().c_str(), newFile);
    refresh();

    // make sure currentTheme stays in bounds
    if (getThemeIndex() >= (int)themes.size()) setThemeIndex(themes.size() - 1);

    return status == FR_OK;
  }
  return false;
}

bool ThemePersistance::createNewTheme(const std::string& name, ThemeFile& theme)
{
  char fullPath[FF_MAX_LFN + 1];
  char* s = strAppend(fullPath, THEMES_PATH, FF_MAX_LFN);
  s = strAppend(s, "/", FF_MAX_LFN - (s - fullPath));
  s = strAppend(s, name.c_str(), FF_MAX_LFN - (s - fullPath));

  if (!isFileAvailable(THEMES_PATH)) {
    FRESULT result = f_mkdir(THEMES_PATH);
    if (result != FR_OK) return false;
  }

  FRESULT result = f_mkdir(fullPath);
  s = strAppend(s, "/", FF_MAX_LFN - (s - fullPath));
  strAppend(s, "theme.yml", FF_MAX_LFN - (s - fullPath));
  if (result == FR_EXIST) {
    if (isFileAvailable(fullPath, true)) {
      POPUP_WARNING(STR_THEME_EXISTS);
      return false;
    }
  } else if (result != FR_OK) return false;
  theme.setPath(fullPath);
  theme.serialize();
  refresh();
  return true;
}

void ThemePersistance::setDefaultTheme(int index)
{
  if (index >= 0 && index < (int)themes.size()) {
    strAppend(g_eeGeneral.selectedTheme, themes[index]->getName().c_str(),
              SELECTED_THEME_NAME_LEN);
    SET_DIRTY();
    currentTheme = index;
  }
}

class DefaultEdgeTxTheme : public ThemeFile
{
 public:
  DefaultEdgeTxTheme() : ThemeFile(THEMES_PATH "/EdgeTX/", false)
  {
    setName("EdgeTX Default");
    setAuthor("EdgeTX Team");
    setInfo("Default EdgeTX Color Scheme");

    // initialize the default color table
    for (uint8_t i = COLOR_THEME_PRIMARY1_INDEX; i < THEME_COLOR_COUNT - 1; i += 1)
      colorList.emplace_back(ColorEntry{(LcdColorIndex)i, defaultColors[i]});
  }
};

void ThemePersistance::insertDefaultTheme()
{
  auto themeFile = new DefaultEdgeTxTheme();
  themes.insert(themes.begin(), themeFile);
}

HeaderDateTime::HeaderDateTime(Window* parent, coord_t x, coord_t y) :
  Window(parent, {x, y, HDR_DATE_WIDTH, HDR_DATE_LINE2 + HDR_DATE_HEIGHT + 2})
{
  date = etx_label_create(lvobj, FONT_XS_INDEX);
  lv_obj_set_pos(date, 0, 0);
  lv_obj_set_size(date, HDR_DATE_WIDTH, HDR_DATE_HEIGHT);
  lv_obj_set_style_text_align(date, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  etx_txt_color(date, COLOR_THEME_PRIMARY2_INDEX);

  time = etx_label_create(lvobj, FONT_XS_INDEX);
  lv_obj_set_pos(time, 0, HDR_DATE_LINE2);
  lv_obj_set_size(time, HDR_DATE_WIDTH, HDR_DATE_HEIGHT);
  lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  etx_txt_color(time, COLOR_THEME_PRIMARY2_INDEX);

  setWindowFlag(NO_CLICK);

  checkEvents();
}

void HeaderDateTime::checkEvents()
{
  const TimerOptions timerOptions = {.options = SHOW_TIME};
  struct gtm t;
  gettime(&t);

  if (t.tm_min != lastTime.tm_min || t.tm_hour != lastTime.tm_hour ||
      t.tm_mday != lastTime.tm_mday || t.tm_mon != lastTime.tm_mon) {
    char str[10];
#if defined(TRANSLATIONS_CN) || defined(TRANSLATIONS_TW)
    sprintf(str, "%02d-%02d", t.tm_mon + 1, t.tm_mday);
#else
    sprintf(str, "%d %s", t.tm_mday, STR_MONTHS[t.tm_mon]);
#endif
    lv_label_set_text(date, str);

    getTimerString(str, getValue(MIXSRC_TX_TIME), timerOptions);
    lv_label_set_text(time, str);

    lastTime = t;
  }
}

void HeaderDateTime::setColor(LcdFlags color)
{
  etx_txt_color_from_flags(date, color);
  etx_txt_color_from_flags(time, color);
}

HeaderIcon::HeaderIcon(Window* parent, EdgeTxIcon icon, std::function<void()> action) :
  StaticIcon(parent, 0, 0, ICON_TOPLEFT_BG, COLOR_THEME_FOCUS_INDEX),
  action(std::move(action))
{
  this->icon = new StaticIcon(this, 0, 0, icon, COLOR_THEME_PRIMARY2_INDEX);
  this->icon->center(width() - PAD_SMALL, height());
#if defined(HARDWARE_TOUCH)
  if (this->action) {
    setWindowFlag(NO_CLICK);
    addCustomButton(0, 0, [=]() { this->action(); });
  }
#endif
}

HeaderIcon::HeaderIcon(Window* parent, const char* iconFile, std::function<void()> action) :
  StaticIcon(parent, 0, 0, ICON_TOPLEFT_BG, COLOR_THEME_FOCUS_INDEX),
  action(std::move(action))
{
  this->icon = new StaticIcon(this, 0, 0, iconFile, COLOR_THEME_PRIMARY2_INDEX);
  this->icon->center(width(), height());
#if defined(HARDWARE_TOUCH)
  if (this->action) {
    setWindowFlag(NO_CLICK);
    addCustomButton(0, 0, [=]() { this->action(); });
  }
#endif
}

HeaderBackIcon::HeaderBackIcon(Window* parent, std::function<void()> action) :
  StaticIcon(parent, LCD_W - PageGroup::PAGE_GROUP_BACK_BTN_XO, 0, ICON_TOPRIGHT_BG, COLOR_THEME_FOCUS_INDEX),
  action(std::move(action))
{
  (new StaticIcon(this, 0, 0, ICON_BTN_CLOSE, COLOR_THEME_PRIMARY2_INDEX))->center(width() + PAD_MEDIUM, height());
#if defined(HARDWARE_TOUCH)
  if (this->action) {
    setWindowFlag(NO_CLICK);
    addCustomButton(0, 0, [=]() { this->action(); });
  }
#endif
}
