// Semantic theme configuration. GPL-2.0-or-later.
#include "theme_config.h"
#include "storage/config_file_workspace.h"
#include "storage/storage.h"
#include <cerrno>

namespace {
constexpr const char* keys[] = {
  "summary/name", "summary/author", "summary/info",
  "colors/PRIMARY1", "colors/PRIMARY2", "colors/PRIMARY3",
  "colors/SECONDARY1", "colors/SECONDARY2", "colors/SECONDARY3",
  "colors/FOCUS", "colors/EDIT", "colors/ACTIVE", "colors/WARNING",
  "colors/DISABLED", "colors/QM_BG", "colors/QM_FG"
};
constexpr unsigned Count = sizeof(keys) / sizeof(*keys);
static_assert(Count == 3 + THEME_COLOR_COUNT - 1, "theme color keys");
char* summary(ThemeConfig& theme, unsigned id, size_t& capacity)
{
  switch (id) {
    case 0: capacity = sizeof(theme.name); return theme.name;
    case 1: capacity = sizeof(theme.author); return theme.author;
    default: capacity = sizeof(theme.info); return theme.info;
  }
}
bool field(void* context, unsigned id, config_stream::Field& f)
{
  if (id >= Count) return false;
  auto& theme = *static_cast<ThemeConfig*>(context);
  strcpy(f.path, keys[id]);
  f.available = true;
  f.required = true;
  if (id >= 3) {
    auto color = theme.colors[id - 3];
    snprintf(f.value, sizeof(f.value), "0x%02X%02X%02X",
             unsigned(GET_RED(color)), unsigned(GET_GREEN(color)), unsigned(GET_BLUE(color)));
  } else {
    size_t capacity;
    auto text = summary(theme, id, capacity);
    char* out = f.value;
    *out++ = '"';
    for (size_t i = 0; i < capacity && text[i]; ++i) {
      unsigned char c = text[i];
      if (out + 5 >= f.value + sizeof(f.value)) return false;
      if (c < 32) { snprintf(out, 5, "\\x%02X", c); out += 4; }
      else { if (c == '"' || c == '\\') *out++ = '\\'; *out++ = c; }
    }
    *out++ = '"'; *out = 0;
  }
  return true;
}
bool set(void* context, unsigned id, const char* scalar)
{
  auto& theme = *static_cast<ThemeConfig*>(context);
  size_t length;
  if (id < 3) {
    size_t capacity;
    auto text = summary(theme, id, capacity);
    if (!config_stream::string(scalar, text, capacity - 1, length)) return false;
    text[length] = 0;
    return true;
  }
  char text[64];
  if (!config_stream::string(scalar, text, sizeof(text) - 1, length)) return false;
  text[length] = 0;
  unsigned long rgb;
  if (length > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    char* end;
    errno = 0;
    rgb = strtoul(text + 2, &end, 16);
    if (errno || *end || rgb > 0xffffff) return false;
  } else {
    int r, g, b, consumed = 0;
    if (sscanf(text, "RGB(%i,%i,%i)%n", &r, &g, &b, &consumed) != 3 ||
        !consumed || text[consumed] || r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
      return false;
    rgb = (unsigned(r) << 16) | (unsigned(g) << 8) | unsigned(b);
  }
  theme.colors[id - 3] = RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
  return true;
}
bool known(void*, const char* path)
{
  if (!strcmp(path, "summary") || !strcmp(path, "colors")) return true;
  for (auto key : keys) if (!strcmp(key, path)) return true;
  return false;
}
using config_file::workspace;
int readByte(void*)
{
  auto& w = workspace;
  if (w.position == w.length) {
    if (f_read(&w.source, w.readBuffer, sizeof(w.readBuffer), &w.length) != FR_OK) return -2;
    w.position = 0;
    if (!w.length) return -1;
  }
  return static_cast<unsigned char>(w.readBuffer[w.position++]);
}
bool writeBytes(void*, const char* text, size_t size)
{
  UINT count;
  return f_write(&workspace.destination, text, size, &count) == FR_OK && count == size;
}
// One serialized transaction shares the existing fixed storage workspace.
const char* recover(const char* path, char* swap)
{
  if (strlen(path) > FF_MAX_LFN) return "theme path too long";
  strcpy(swap, path); strcat(swap, ".swap");
  auto r = f_stat(path, &workspace.fileInfo);
  if (r == FR_OK) return nullptr;
  if (r != FR_NO_FILE && r != FR_NO_PATH) return SDCARD_ERROR(r);
  r = f_stat(swap, &workspace.fileInfo);
  if (r == FR_NO_FILE || r == FR_NO_PATH) return nullptr;
  if (r == FR_OK) r = f_rename(swap, path);
  return r == FR_OK ? nullptr : SDCARD_ERROR(r);
}
}

const char* loadThemeConfig(const char* path, ThemeConfig& theme)
{
  if (config_file::busy) return "configuration busy";
  config_file::Guard guard;
  char swap[FF_MAX_LFN + 6];
  if (auto error = recover(path, swap)) return error;
  auto r = f_open(&workspace.source, path, FA_READ | FA_OPEN_EXISTING);
  if (r != FR_OK) return SDCARD_ERROR(r);
  ThemeConfig candidate = theme;
  workspace.position = workspace.length = 0;
  config_stream::Schema schema{Count, &candidate, field, set, known};
  auto result = config_stream::process({nullptr, readByte, nullptr}, {}, schema, workspace.parser, true);
  r = f_close(&workspace.source);
  if (!result) return result.error;
  if (r != FR_OK) return SDCARD_ERROR(r);
  if (result.invalid) return "invalid theme value";
  theme = candidate;
  return nullptr;
}
const char* saveThemeConfig(const char* path, ThemeConfig& theme)
{
  if (config_file::busy) return "configuration busy";
  config_file::Guard guard;
  char swap[FF_MAX_LFN + 6], temporary[FF_MAX_LFN + 5];
  if (auto error = recover(path, swap)) return error;
  strcpy(temporary, path); strcat(temporary, ".tmp");
  auto r = f_open(&workspace.destination, temporary, FA_CREATE_ALWAYS | FA_WRITE);
  if (r != FR_OK) return SDCARD_ERROR(r);
  config_stream::Schema schema{Count, &theme, field, set, known};
  // Retain the document marker required by older theme readers.
  bool ok = writeBytes(nullptr, "---\n", 4);
  auto result = config_stream::process({}, {nullptr, nullptr, writeBytes}, schema, workspace.parser, false);
  ok = ok && bool(result);
  if (f_sync(&workspace.destination) != FR_OK) ok = false;
  if (f_close(&workspace.destination) != FR_OK) ok = false;
  if (!ok) { f_unlink(temporary); return "theme write failed"; }
  r = f_stat(path, &workspace.fileInfo);
  bool exists = r == FR_OK;
  if (!exists && r != FR_NO_FILE && r != FR_NO_PATH) return SDCARD_ERROR(r);
  if (exists) {
    r = f_stat(swap, &workspace.fileInfo);
    if (r == FR_OK) r = f_unlink(swap);
    else if (r == FR_NO_FILE) r = FR_OK;
    if (r != FR_OK) return SDCARD_ERROR(r);
    r = f_rename(path, swap);
    if (r != FR_OK) return SDCARD_ERROR(r);
  }
  r = f_rename(temporary, path);
  if (r != FR_OK) {
    if (exists) f_rename(swap, path);
    return SDCARD_ERROR(r);
  }
  if (exists) f_unlink(swap);
  return nullptr;
}
