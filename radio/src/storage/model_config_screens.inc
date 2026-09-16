// Screen storage belongs to the candidate until commit, including all strings
// and option vectors. Every index and string is bounded by the schema.
#if defined(COLORLCD)
static CustomScreenData stagedScreens[MAX_CUSTOM_SCREENS];
static TopBarPersistentData stagedTopbar;
// Preserve each typed union alternative until its discriminator is known.
// Only options actually present allocate slots, each capped at
// MAX_WIDGET_OPTIONS.
struct OptionStage {
  uint32_t values[6]{};
  uint8_t seen = 0;
};
static std::vector<OptionStage> widgetStages[MAX_CUSTOM_SCREENS]
                                            [MAX_LAYOUT_ZONES];
static std::vector<OptionStage> topbarStages[MAX_TOPBAR_ZONES];
static OptionStage layoutStages[MAX_CUSTOM_SCREENS][MAX_LAYOUT_OPTIONS];
OptionStage* widgetStage(WidgetPersistentData& widget, unsigned option)
{
  std::vector<OptionStage>* stages = nullptr;
  for (unsigned i = 0; i < MAX_CUSTOM_SCREENS; ++i)
    for (unsigned j = 0; j < MAX_LAYOUT_ZONES; ++j)
      if (&widget == &stagedScreens[i].layoutData.zones[j].widgetData)
        stages = &widgetStages[i][j];
  for (unsigned i = 0; i < MAX_TOPBAR_ZONES; ++i)
    if (&widget == &stagedTopbar.zones[i].widgetData) stages = &topbarStages[i];
  if (!stages) return nullptr;
  if (stages->size() <= option) stages->resize(option + 1);
  return &(*stages)[option];
}
void stageOption(WidgetPersistentData& widget, unsigned option, unsigned part)
{
  if (auto* stage = widgetStage(widget, option)) {
    stage->values[part - 1] = widget.options[option].value.unsignedValue;
    stage->seen |= 1u << (part - 1);
  }
}
void clearScreenStages()
{
  for (unsigned i = 0; i < MAX_CUSTOM_SCREENS; ++i) {
    for (auto& stages : widgetStages[i]) stages.clear();
    for (auto& stage : layoutStages[i]) stage = OptionStage{};
  }
  for (auto& stages : topbarStages) stages.clear();
}
void resolveWidget(WidgetPersistentData& widget,
                   const std::vector<OptionStage>& stages)
{
  for (unsigned i = 0; i < widget.options.size(); ++i) {
    auto& option = widget.options[i];
    unsigned type = option.type;
    if (type == WOV_String) continue;
    option.value.unsignedValue =
        i < stages.size() && (stages[i].seen & (1u << type))
            ? stages[i].values[type]
            : 0;
  }
}
void resolveScreenStages()
{
  for (unsigned i = 0; i < MAX_CUSTOM_SCREENS; ++i) {
    for (unsigned j = 0; j < MAX_LAYOUT_ZONES; ++j)
      resolveWidget(stagedScreens[i].layoutData.zones[j].widgetData,
                    widgetStages[i][j]);
    for (unsigned j = 0; j < MAX_LAYOUT_OPTIONS; ++j) {
      auto& option = stagedScreens[i].layoutData.options[j];
      auto& stage = layoutStages[i][j];
      option.value.unsignedValue =
          (stage.seen & (1u << option.type)) ? stage.values[option.type] : 0;
    }
  }
  for (unsigned i = 0; i < MAX_TOPBAR_ZONES; ++i)
    resolveWidget(stagedTopbar.zones[i].widgetData, topbarStages[i]);
}
CustomScreenData& screenFor(ModelData& m, unsigned i)
{
  return &m == &candidate ? stagedScreens[i] : *m.getScreenData(i);
}
TopBarPersistentData& topbarFor(ModelData& m)
{
  return &m == &candidate ? stagedTopbar : *m.getTopbarData();
}
WidgetOptionValueTyped& optionFor(WidgetPersistentData& w, unsigned k)
{
  if (w.options.size() <= k) w.options.resize(k + 1);
  return w.options[k];
}
bool stringValue(std::string& value, Field& f, const char* t)
{
  if (!t) return quoted(f.value, sizeof(f.value), value.c_str(), value.size());
  if (*t && strchr("[{|>", *t)) return false;
  char s[MaxValue];
  size_t len;
  if (!string(t, s, sizeof(s) - 1, len)) return false;
  value.assign(s, len);
  return true;
}
const Enum WidgetTypes[] = {{"Unsigned", 0}, {"Signed", 1}, {"Bool", 2},
                            {"String", 3},   {"Source", 4}, {"Color", 5},
                            {nullptr, 0}};
const Enum LayoutTypes[] = {
    {"None", 0}, {"Bool", 1}, {"Color", 2}, {nullptr, 0}};
bool colorValue(uint32_t& value, Field& f, const char* t)
{
  if (t) {
    int64_t n;
    if (!strncmp(t, "COLIDX", 6) && integer(t + 6, 0, 65535, n)) {
      value = COLOR2FLAGS(n);
      return true;
    }
    if (strlen(t) != 8 || strncmp(t, "0x", 2)) return false;
    uint32_t rgb = 0;
    for (unsigned i = 2; i < 8; ++i) {
      char c = t[i];
      unsigned d = c >= '0' && c <= '9'   ? c - '0'
                   : c >= 'A' && c <= 'F' ? c - 'A' + 10
                   : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                          : 16;
      if (d == 16) return false;
      rgb = rgb * 16 + d;
    }
    value = RGB2FLAGS((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
    return true;
  }
  if (value & RGB_FLAG) {
    unsigned c = COLOR_VAL(value);
    snprintf(f.value, sizeof(f.value), "0x%02X%02X%02X", GET_RED(c),
             GET_GREEN(c), GET_BLUE(c));
  } else
    snprintf(f.value, sizeof(f.value), "COLIDX%u", unsigned(COLOR_VAL(value)));
  return true;
}
bool widgetValue(WidgetPersistentData& w, unsigned k, unsigned part, Field& f,
                 const char* t)
{
  auto& o = optionFor(w, k);
  int64_t v;
  if (part == 0) {
    if (t) {
      if (!enumeration(WidgetTypes, t, v) || v < 0 || v > 5) return false;
      o.type = WidgetOptionValueEnum(v);
      return true;
    }
    return enumOutput(WidgetTypes, o.type, f);
  }
  if (part == 4) return stringValue(o.value.stringValue, f, t);
  if (part == 6) {
    if (!colorValue(o.value.unsignedValue, f, t)) return false;
    if (t) stageOption(w, k, part);
    return true;
  }
  if (part == 5) {
    if (t) {
      if (!sourceInput(t, v)) return false;
      o.value.unsignedValue = v;
      stageOption(w, k, part);
      return true;
    }
    char b[64];
    return sourceOutput(o.value.unsignedValue, b, sizeof(b)) &&
           quoted(f.value, sizeof(f.value), b, strlen(b));
  }
  if (t) {
    if (!integer(t, part == 2 ? INT32_MIN : 0,
                 part == 2   ? INT32_MAX
                 : part == 3 ? 1
                             : UINT32_MAX,
                 v))
      return false;
    o.value.unsignedValue = v;
    stageOption(w, k, part);
    return true;
  }
  return formatInteger(f.value, sizeof(f.value),
                       part == 2 ? int64_t(o.value.signedValue)
                                 : int64_t(o.value.unsignedValue));
}
bool layoutScalar(LayoutOptionValueTyped& o, unsigned part, Field& f,
                  const char* t)
{
  int64_t v;
  if (part == 0) {
    if (t) {
      if (!enumeration(LayoutTypes, t, v) || v < 0 || v > 2) return false;
      o.type = LayoutOptionValueEnum(v);
      return true;
    }
    return enumOutput(LayoutTypes, o.type, f);
  }
  if (part == 3) return colorValue(o.value.unsignedValue, f, t);
  if (t) {
    if (!integer(t, 0, part == 2 ? 1 : UINT32_MAX, v)) return false;
    o.value.unsignedValue = v;
    return true;
  }
  return formatInteger(f.value, sizeof(f.value), o.value.unsignedValue);
}
bool layoutValue(LayoutOptionValueTyped& option, unsigned part, Field& f,
                 const char* text)
{
  if (!layoutScalar(option, part, f, text)) return false;
  if (text && part) {
    for (unsigned i = 0; i < MAX_CUSTOM_SCREENS; ++i)
      for (unsigned j = 0; j < MAX_LAYOUT_OPTIONS; ++j)
        if (&option == &stagedScreens[i].layoutData.options[j]) {
          auto& stage = layoutStages[i][j];
          stage.values[part - 1] = option.value.unsignedValue;
          stage.seen |= 1u << (part - 1);
        }
  }
  return true;
}
bool screenRelevant(const Context& c, const char* path, unsigned i, unsigned j,
                    unsigned k)
{
  bool top = !strncmp(path, "topbarData/", 11);
  ModelData& m = *c.model;
  if (!top) {
    bool exists =
        c.loading ? !stagedScreens[i].LayoutId.empty() : m.hasScreenData(i);
    if (!exists) return false;
    auto& s = screenFor(m, i);
    if (!strstr(path, "/zones/")) {
      if (!strstr(path, "/options/")) return true;
      auto type = s.layoutData.options[j].type;
      if (!strstr(path, "/value/")) return true;
      return strstr(path, type == LOV_Bool    ? "/boolValue"
                          : type == LOV_Color ? "/color"
                                              : "/unsignedValue") != nullptr;
    }
  }
  auto& zone =
      top ? topbarFor(m).zones[i] : screenFor(m, i).layoutData.zones[j];
  if (zone.widgetName.empty()) return false;
  if (!strstr(path, "/options/")) return true;
  unsigned n = top ? j : k;
  if (n >= zone.widgetData.options.size()) return false;
  if (!strstr(path, "/value/")) return true;
  const char* names[] = {"/unsignedValue", "/signedValue", "/boolValue",
                         "/stringValue",   "/source",      "/color"};
  unsigned type = zone.widgetData.options[n].type;
  return type < 6 && strstr(path, names[type]);
}
#define ACCESS_LayoutId(member, lo, hi)                 \
  {                                                     \
    auto& value = screenFor(m, i).LayoutId;             \
    if (!stringValue(value, f, t)) return false;        \
    if (t && value == "Layout6x1") value = "Layout1x6"; \
    return true;                                        \
  }
#define ACCESS_WidgetName(member, lo, hi) \
  return stringValue(screenFor(m, i).layoutData.zones[j].widgetName, f, t);
#define ACCESS_TopbarName(member, lo, hi) \
  return stringValue(topbarFor(m).zones[i].widgetName, f, t);
#define ACCESS_LayoutOptionType(member, lo, hi) \
  return layoutValue(screenFor(m, i).layoutData.options[j], 0, f, t);
#define ACCESS_LayoutUnsigned(member, lo, hi) \
  return layoutValue(screenFor(m, i).layoutData.options[j], 1, f, t);
#define ACCESS_LayoutBool(member, lo, hi) \
  return layoutValue(screenFor(m, i).layoutData.options[j], 2, f, t);
#define ACCESS_LayoutColor(member, lo, hi) \
  return layoutValue(screenFor(m, i).layoutData.options[j], 3, f, t);
#define WIDGET_ACCESS(part)                                                   \
  return widgetValue(screenFor(m, i).layoutData.zones[j].widgetData, k, part, \
                     f, t);
#define TOPBAR_ACCESS(part) \
  return widgetValue(topbarFor(m).zones[i].widgetData, j, part, f, t);
#define ACCESS_WidgetType(m, l, h) WIDGET_ACCESS(0)
#define ACCESS_WidgetUnsigned(m, l, h) WIDGET_ACCESS(1)
#define ACCESS_WidgetSigned(m, l, h) WIDGET_ACCESS(2)
#define ACCESS_WidgetBool(m, l, h) WIDGET_ACCESS(3)
#define ACCESS_WidgetString(m, l, h) WIDGET_ACCESS(4)
#define ACCESS_WidgetSource(m, l, h) WIDGET_ACCESS(5)
#define ACCESS_WidgetColor(m, l, h) WIDGET_ACCESS(6)
#define ACCESS_TopbarWidgetType(m, l, h) TOPBAR_ACCESS(0)
#define ACCESS_TopbarWidgetUnsigned(m, l, h) TOPBAR_ACCESS(1)
#define ACCESS_TopbarWidgetSigned(m, l, h) TOPBAR_ACCESS(2)
#define ACCESS_TopbarWidgetBool(m, l, h) TOPBAR_ACCESS(3)
#define ACCESS_TopbarWidgetString(m, l, h) TOPBAR_ACCESS(4)
#define ACCESS_TopbarWidgetSource(m, l, h) TOPBAR_ACCESS(5)
#define ACCESS_TopbarWidgetColor(m, l, h) TOPBAR_ACCESS(6)
#endif
