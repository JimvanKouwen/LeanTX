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
  f.available = true;
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
uint8_t seen[(Count + 7) / 8];
void enter(void* ctx, const config_stream::Node& parent, const char* key, const char* text,
           config_stream::Node& child, config_stream::Result& result)
{
  child.section = -1;
  if (parent.section == 0) {
    if (!strcmp(key, "summary")) child.section = 1;
    if (!strcmp(key, "colors")) child.section = 2;
    if (child.section >= 0 && *text) result.error = "expected theme mapping";
    return;
  }
  unsigned first = parent.section == 1 ? 0 : 3;
  unsigned last = parent.section == 1 ? 3 : parent.section == 2 ? Count : 3;
  for (unsigned id = first; id < last; ++id) if (!strcmp(key, strchr(keys[id], '/') + 1)) {
    if (!config_stream::mark(seen, id, sizeof(seen), result)) return;
    if (!set(ctx, id, text)) ++result.invalid;
    return;
  }
}
void save(void* ctx, config_stream::Writer& writer, config_stream::Field& f)
{
  for (unsigned section = 0; section < 2; ++section) {
    writer.begin(section ? "colors" : "summary");
    for (unsigned id = section ? 3 : 0; id < (section ? Count : 3); ++id) {
      if (!field(ctx, id, f)) { writer.error = "invalid theme value"; return; }
      writer.value(strchr(keys[id], '/') + 1, f.value);
    }
    writer.end();
  }
}
}
const char* loadThemeConfig(const char* path, ThemeConfig& theme)
{
  if (config_file::busy) return "configuration busy";
  config_file::Guard guard;
  if (const char* error = config_file::read(path)) return error;
  auto& w = config_file::workspace;
  ThemeConfig candidate = theme;
  config_stream::Document schema{&candidate, enter, save, seen, sizeof(seen)};
  auto result = config_stream::parse(w.parser.document, w.length, schema, w.parser);
  if (!result) return result.error;
  if (result.invalid) return "invalid theme value";
  theme = candidate;
  return nullptr;
}
const char* saveThemeConfig(const char* path, ThemeConfig& theme)
{
  if (config_file::busy) return "configuration busy";
  config_file::Guard guard;
  config_stream::Document schema{&theme, enter, save, seen, sizeof(seen)};
  return config_file::save(path, schema);
}
