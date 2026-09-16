// Semantic theme configuration. GPL-2.0-or-later.
#pragma once
#include "theme_manager.h"

struct ThemeConfig {
  char name[SELECTED_THEME_NAME_LEN + 1]{};
  char author[ThemeFile::AUTHOR_LENGTH + 1]{};
  char info[ThemeFile::INFO_LENGTH + 1]{};
  uint32_t colors[THEME_COLOR_COUNT - 1]{};
};
const char* loadThemeConfig(const char* path, ThemeConfig& theme);
const char* saveThemeConfig(const char* path, ThemeConfig& theme);
