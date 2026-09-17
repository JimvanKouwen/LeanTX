// LeanTX semantic model adapter. GPL-2.0-or-later.
#include "model_config_adapter.h"

#include <stdio.h>
#include <string.h>

#include "analogs.h"
#include "edgetx.h"
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"
#include "stamp.h"
#include "switches.h"

namespace model_config
{
namespace
{
using namespace config_stream;
struct Enum {
  const char* name;
  int value;
};
const Enum TimerMode[] = {{"OFF", 0},  {"ON", 1},      {"START", 2},
                          {"THR", 3},  {"THR_REL", 4}, {"THR_START", 5},
                          {nullptr, 0}};
const Enum Module[] = {{"TYPE_NONE", MODULE_TYPE_NONE},
                       {"TYPE_CROSSFIRE", MODULE_TYPE_CROSSFIRE},
                       {nullptr, 0}};
const Enum Antenna[] = {{"MODE_INTERNAL", -2},
                        {"MODE_ASK", -1},
                        {"MODE_PER_MODEL", 0},
                        {"MODE_EXTERNAL", 1},
                        {nullptr, 0}};
const Enum PotsMode[] = {
    {"WARN_OFF", 0}, {"WARN_MANUAL", 1}, {"WARN_AUTO", 2}, {nullptr, 0}};
const Enum Override[] = {{"GLOBAL", 0}, {"OFF", 1}, {"ON", 2}, {nullptr, 0}};
const Enum SensorType[] = {
    {"TYPE_CUSTOM", 0}, {"TYPE_CALCULATED", 1}, {nullptr, 0}};
const Enum Formula[] = {{"FORMULA_ADD", 0},      {"FORMULA_AVERAGE", 1},
                        {"FORMULA_MIN", 2},      {"FORMULA_MAX", 3},
                        {"FORMULA_MULTIPLY", 4}, {"FORMULA_TOTALIZE", 5},
                        {"FORMULA_CELL", 6},     {"FORMULA_CONSUMPTION", 7},
                        {"FORMULA_DIST", 8},     {nullptr, 0}};
const Enum JoystickMode[] = {
    {"JOYSTICK", 0}, {"GAMEPAD", 1}, {"MULTIAXIS", 2}, {nullptr, 0}};
const Enum JoystickChannel[] = {{"CH_NONE", 0},
                                {"CH_BUTTON", 1},
                                {"CH_AXIS", 2},
                                {"CH_SIM", 3},
                                {nullptr, 0}};
const Enum Hats[] = {{"TRIMS_ONLY", 0},
                     {"KEYS_ONLY", 1},
                     {"SWITCHABLE", 2},
                     {"GLOBAL", 3},
                     {nullptr, 0}};
const Enum SwitchWarning[] = {
    {"none", 0}, {"up", 1}, {"mid", 2}, {"down", 3}, {nullptr, 0}};
#if defined(FUNCTION_SWITCHES)
const Enum FunctionType[] = {
    {"NONE", SWITCH_NONE},     {"TOGGLE", SWITCH_TOGGLE},
    {"2POS", SWITCH_2POS},     {"3POS", SWITCH_3POS},
    {"GLOBAL", SWITCH_GLOBAL}, {"none", SWITCH_NONE},
    {"toggle", SWITCH_TOGGLE}, {"2pos", SWITCH_2POS},
    {"3pos", SWITCH_3POS},     {nullptr, 0}};
#endif
const Enum FunctionStart[] = {
    {"START_OFF", 0}, {"START_ON", 1}, {"START_PREVIOUS", 2}, {nullptr, 0}};
const Enum BooleanMode[] = {{"OFF", 0}, {"ON", 1}, {nullptr, 0}};
const Enum sources[] = {{"NONE", MIXSRC_NONE},
                        {"MIN", MIXSRC_MIN},
                        {"MAX", MIXSRC_MAX},
                        {"TX_VOLTAGE", MIXSRC_TX_VOLTAGE},
                        {"TX_TIME", MIXSRC_TX_TIME},
                        {"TX_GPS", MIXSRC_TX_GPS},
                        {nullptr, 0}};
const Enum switches[] = {{"NONE", SWSRC_NONE},
                         {"ON", SWSRC_ON},
                         {"ONE", SWSRC_ONE},
                         {"OFF", SWSRC_OFF},
                         {"TELEMETRY_STREAMING", SWSRC_TELEMETRY_STREAMING},
                         {"RADIO_ACTIVITY", SWSRC_RADIO_ACTIVITY},
                         {nullptr, 0}};

bool quoted(char* out, size_t capacity, const char* text, size_t length)
{
  size_t pos = 0;
  if (capacity < 3) return false;
  out[pos++] = '"';
  const char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < length && text[i]; ++i) {
    unsigned char c = text[i];
    if (c < 32 || c == '"' || c == '\\') {
      if (pos + 5 >= capacity) return false;
      out[pos++] = '\\';
      out[pos++] = 'x';
      out[pos++] = hex[c >> 4];
      out[pos++] = hex[c & 15];
    } else {
      if (pos + 2 >= capacity) return false;
      out[pos++] = c;
    }
  }
  out[pos++] = '"';
  out[pos] = 0;
  return true;
}
template <size_t N>
bool textValue(char (&value)[N], Field& f, const char* text)
{
  if (!text) return quoted(f.value, sizeof(f.value), value, N);
  if (*text && strchr("[{|>", *text)) return false;
  auto& decoded = scalarBuffer;
  size_t len;
  if (!string(text, decoded, sizeof(decoded), len)) return false;
  memset(value, 0, N);
  memcpy(value, decoded, len < N ? len : N);
  return true;
}
bool enumeration(const Enum* table, const char* text, int64_t& n)
{
  auto& decoded = scalarBuffer;
  size_t len;
  if (!string(text, decoded, sizeof(decoded) - 1, len)) return false;
  decoded[len] = 0;
  for (; table->name; ++table)
    if (!strcmp(decoded, table->name)) {
      n = table->value;
      return true;
    }
  return integer(decoded, INT32_MIN, UINT32_MAX, n);
}
bool enumOutput(const Enum* table, int64_t n, Field& f)
{
  for (; table->name; ++table)
    if (table->value == n) {
      strcpy(f.value, table->name);
      return true;
    }
  return formatInteger(f.value, sizeof(f.value), n);
}
// Canonical names keep source IDs independent of target enum numbering.
bool sourceOutput(int n, char* out, size_t cap)
{
  if (n < 0) {
    if (cap < 2) return false;
    *out++ = '!';
    --cap;
    n = -n;
  }
  for (auto e = sources; e->name; ++e)
    if (e->value == n) {
      snprintf(out, cap, "%s", e->name);
      return true;
    }
  if (n >= MIXSRC_FIRST_STICK && n <= MIXSRC_LAST_STICK)
    snprintf(out, cap, "%s",
             analogGetCanonicalName(ADC_INPUT_MAIN, n - MIXSRC_FIRST_STICK));
  else if (n >= MIXSRC_FIRST_POT && n <= MIXSRC_LAST_POT)
    snprintf(out, cap, "%s",
             analogGetCanonicalName(ADC_INPUT_FLEX, n - MIXSRC_FIRST_POT));
  else if (n >= MIXSRC_FIRST_SWITCH && n <= MIXSRC_LAST_SWITCH)
    snprintf(out, cap, "%s", switchGetDefaultName(n - MIXSRC_FIRST_SWITCH));
#if defined(FUNCTION_SWITCHES)
  else if (n >= MIXSRC_FIRST_CUSTOMSWITCH_GROUP &&
           n <= MIXSRC_LAST_CUSTOMSWITCH_GROUP)
    snprintf(out, cap, "GR%d", n - MIXSRC_FIRST_CUSTOMSWITCH_GROUP + 1);
#endif
  else if (n >= MIXSRC_FIRST_CH && n <= MIXSRC_LAST_CH)
    snprintf(out, cap, "ch(%d)", n - MIXSRC_FIRST_CH);
  else if (n >= MIXSRC_FIRST_TIMER && n <= MIXSRC_LAST_TIMER)
    snprintf(out, cap, "Tmr%d", n - MIXSRC_FIRST_TIMER + 1);
  else if (n >= MIXSRC_FIRST_TELEM && n <= MIXSRC_LAST_TELEM) {
    int x = n - MIXSRC_FIRST_TELEM;
    snprintf(out, cap, "tele(%s%d)",
             x % 3 == 1   ? "-"
             : x % 3 == 2 ? "+"
                          : "",
             x / 3);
  } else
    return formatInteger(out, cap, n);
  return true;
}
bool sourceInput(const char* text, int64_t& value)
{
  auto& s = scalarBuffer;
  size_t len;
  if (!string(text, s, sizeof(s) - 1, len)) return false;
  s[len] = 0;
  const char* p = s;
  int sign = 1;
  if (*p == '!') {
    sign = -1;
    ++p;
  }
  int n = -1, a = 0, used = 0;
  for (auto e = sources; e->name; ++e)
    if (!strcmp(p, e->name)) n = e->value;
  used = 0;
  if (n < 0 && sscanf(p, "ch(%d)%n", &a, &used) == 1 && used && !p[used] &&
      a >= 0 && a < MAX_OUTPUT_CHANNELS)
    n = MIXSRC_FIRST_CH + a;
  used = 0;
  if (n < 0 && sscanf(p, "Tmr%d%n", &a, &used) == 1 && !p[used] && a > 0 &&
      a <= MAX_TIMERS)
    n = MIXSRC_FIRST_TIMER + a - 1;
  used = 0;
  if (n < 0 && !strncmp(p, "tele(", 5)) {
    const char* t = p + 5;
    int variant = *t == '-' ? 1 : *t == '+' ? 2 : 0;
    if (variant) ++t;
    if (sscanf(t, "%d)%n", &a, &used) == 1 && used && !t[used] && a >= 0 &&
        a < MAX_TELEMETRY_SENSORS)
      n = MIXSRC_FIRST_TELEM + 3 * a + variant;
  }
#if defined(FUNCTION_SWITCHES)
  used = 0;
  if (n < 0 && sscanf(p, "GR%d%n", &a, &used) == 1 && !p[used] && a > 0 &&
      a <= NUM_FUNCTIONS_GROUPS)
    n = MIXSRC_FIRST_CUSTOMSWITCH_GROUP + a - 1;
#endif
  const char* legacy[] = {"Rud", "Ele", "Thr", "Ail"};
  for (unsigned i = 0; i < 4; ++i)
    if (n < 0 && !strcmp(p, legacy[i])) n = MIXSRC_FIRST_STICK + i;
  used = 0;
  if (n < 0 && sscanf(p, "POT%d%n", &a, &used) == 1 && !p[used] && a > 0 &&
      a <= MAX_POTS)
    n = MIXSRC_FIRST_POT + a - 1;
  if (n < 0) {
    a = analogLookupCanonicalIdx(ADC_INPUT_MAIN, p, strlen(p));
    if (a >= 0) n = MIXSRC_FIRST_STICK + a;
  }
  if (n < 0) {
    a = analogLookupCanonicalIdx(ADC_INPUT_FLEX, p, strlen(p));
    if (a >= 0) n = MIXSRC_FIRST_POT + a;
  }
  if (n < 0) {
    a = switchLookupIdx(p, strlen(p));
    if (a >= 0) n = MIXSRC_FIRST_SWITCH + a;
  }
  if (n < 0) return integer(s, -32768, 65535, value);
  value = sign * n;
  return true;
}
bool switchOutput(int n, Field& f)
{
  char s[48];
  const char* sign = n < 0 ? "!" : "";
  if (n < 0) n = -n;
  for (auto e = switches; e->name; ++e)
    if (e->value == n) {
      snprintf(s, sizeof(s), "%s%s", sign, e->name);
      return quoted(f.value, sizeof(f.value), s, strlen(s));
    }
  if (n >= SWSRC_FIRST_SWITCH && n <= SWSRC_LAST_SWITCH) {
    int x = n - SWSRC_FIRST_SWITCH;
    snprintf(s, sizeof(s), "%s%s%d", sign, switchGetDefaultName(x / 3), x % 3);
  } else if (n >= SWSRC_FIRST_MULTIPOS_SWITCH &&
             n <= SWSRC_LAST_MULTIPOS_SWITCH) {
    int x = n - SWSRC_FIRST_MULTIPOS_SWITCH;
    snprintf(s, sizeof(s), "%s6P%d%d", sign, x / XPOTS_MULTIPOS_COUNT,
             x % XPOTS_MULTIPOS_COUNT);
  } else if (n >= SWSRC_FIRST_TRIM && n <= SWSRC_LAST_TRIM) {
    int x = n - SWSRC_FIRST_TRIM;
    snprintf(s, sizeof(s), "%sTrimT%d%s", sign, x / 2 + 1,
             x % 2 ? "Up" : "Down");
  } else if (n >= SWSRC_FIRST_SENSOR && n <= SWSRC_LAST_SENSOR)
    snprintf(s, sizeof(s), "%sT%d", sign, n - SWSRC_FIRST_SENSOR + 1);
  else
    return formatInteger(f.value, sizeof(f.value), *sign ? -n : n);
  return quoted(f.value, sizeof(f.value), s, strlen(s));
}
bool switchInput(const char* text, int64_t& value)
{
  auto& s = scalarBuffer;
  size_t len;
  if (!string(text, s, sizeof(s) - 1, len)) return false;
  s[len] = 0;
  char* p = s;
  int sign = 1;
  if (*p == '!') {
    sign = -1;
    ++p;
  }
  for (auto e = switches; e->name; ++e)
    if (!strcmp(p, e->name)) {
      value = sign * e->value;
      return true;
    }
  size_t l = strlen(p);
  if (l > 1 && p[l - 1] >= '0' && p[l - 1] <= '2') {
    int idx = switchLookupIdx(p, l - 1);
    if (idx >= 0) {
      value = sign * (SWSRC_FIRST_SWITCH + idx * 3 + p[l - 1] - '0');
      return true;
    }
  }
  if (strlen(p) == 4 && p[0] == '6' && p[1] == 'P' && p[2] >= '0' &&
      p[2] <= '9' && p[3] >= '0' && p[3] < '0' + XPOTS_MULTIPOS_COUNT) {
    value = sign * (SWSRC_FIRST_MULTIPOS_SWITCH +
                    (p[2] - '0') * XPOTS_MULTIPOS_COUNT + p[3] - '0');
    return true;
  }
  int a, used = 0;
  if (sscanf(p, "T%d%n", &a, &used) == 1 && !p[used] && a > 0 &&
      a <= MAX_TELEMETRY_SENSORS) {
    value = sign * (SWSRC_FIRST_SENSOR + a - 1);
    return true;
  }
  for (int i = 0; i < MAX_TRIMS * 2; ++i) {
    char name[24];
    snprintf(name, sizeof(name), "TrimT%d%s", i / 2 + 1, i % 2 ? "Up" : "Down");
    if (!strcmp(p, name)) {
      value = sign * (SWSRC_FIRST_TRIM + i);
      return true;
    }
  }
  return integer(s, -512, 511, value);
}
struct Context {
  ModelData* model;
  bool loading;
};
static ModelData candidate, defaults;
static Field defaultField;
void initialize(ModelData& model);
static Context loadContext{&candidate, true}, saveContext;
#if defined(COLORLCD)
static uint8_t seen[8192];
#else
static uint8_t seen[2048];
#endif
#include "model_config_screens.inc"
#include "model_config_switches.inc"
struct SensorStage {
  uint32_t values[16];
};
static SensorStage sensorStage[MAX_TELEMETRY_SENSORS];
int sensorSlot(const char* path, unsigned j)
{
  const char* suffix = strstr(path, "/id1/");
  if (suffix) return !strcmp(suffix, "/id1/id") ? 0 : 1;
  suffix = strstr(path, "/id2/");
  if (suffix) return !strcmp(suffix, "/id2/instance") ? 2 : 3;
  suffix = strstr(path, "/cfg/");
  if (!suffix) return -1;
  const char* names[] = {
      "custom/ratio",       "custom/offset", "cell/source", "cell/index",
      "consumption/source", "dist/gps",      "dist/alt",    "param"};
  for (unsigned k = 0; k < 8; ++k)
    if (!strcmp(suffix + 5, names[k])) return k + 4;
  return strstr(suffix, "calc/sources/") ? 12 + j : -1;
}
bool relevant(const Context& c, const char* path, unsigned i, unsigned j,
              unsigned k)
{
#if defined(COLORLCD)
  if (!strncmp(path, "screenData/", 11) || !strncmp(path, "topbarData/", 11))
    return screenRelevant(c, path, i, j, k);
#endif
#if defined(FUNCTION_SWITCHES)
  if (!strncmp(path, "customSwitches/", 15)) {
    int index = functionIndex(*c.model, i);
    return index >= 0 && index < NUM_FUNCTIONS_SWITCHES;
  }
#endif
  if (!strcmp(path, "header/modelId")) return false;
  if (!strncmp(path, "cfsGroupOn/", 11)) return i > 0;
  if (!strncmp(path, "telemetrySensors/", 17) && i < MAX_TELEMETRY_SENSORS) {
    const auto& sensor = c.model->telemetrySensors[i];
    int slot = sensorSlot(path, j);
    if (slot < 0) return true;
    if (slot < 2)
      return slot ==
             ((sensor.type == TELEM_TYPE_CALCULATED && sensor.persistent) ? 1
                                                                          : 0);
    if (slot < 4) return slot == (sensor.type == TELEM_TYPE_CALCULATED ? 3 : 2);
    if (sensor.unit >= UNIT_FIRST_VIRTUAL) return slot == 11;
    if (sensor.type != TELEM_TYPE_CALCULATED) return slot == 4 || slot == 5;
    if (sensor.formula == TELEM_FORMULA_CELL) return slot == 6 || slot == 7;
    if (sensor.formula == TELEM_FORMULA_DIST) return slot == 9 || slot == 10;
    if (sensor.formula == TELEM_FORMULA_CONSUMPTION ||
        sensor.formula == TELEM_FORMULA_TOTALIZE)
      return slot == 8;
    return slot >= 12;
  }
  return true;
}

#define ACCESS_Version(member, lo, hi)               \
  if (t) return true;                                \
  snprintf(f.value, sizeof(f.value), "%s", VERSION); \
  return true;
#define CAP_Version(member) 0
#define ACCESS_Number(member, lo, hi)         \
  if (t) {                                    \
    int64_t v;                                \
    if (!integer(t, lo, hi, v)) return false; \
    m.member = v;                             \
    return true;                              \
  }                                           \
  return formatInteger(f.value, sizeof(f.value), m.member);
#define ACCESS_Text(member, lo, hi) return textValue(m.member, f, t);
#define ACCESS_ENUM(member, lo, hi, table)                           \
  if (t) {                                                           \
    int64_t v;                                                       \
    if (!enumeration(table, t, v) || v < lo || v > hi) return false; \
    m.member = v;                                                    \
    return true;                                                     \
  }                                                                  \
  return enumOutput(table, m.member, f);
#define ACCESS_TimerMode(m, l, h) ACCESS_ENUM(m, l, h, TimerMode)
#define ACCESS_Module(m, l, h) ACCESS_ENUM(m, l, h, Module)
#define ACCESS_Antenna(m, l, h) ACCESS_ENUM(m, l, h, Antenna)
#define ACCESS_PotsMode(m, l, h) ACCESS_ENUM(m, l, h, PotsMode)
#define ACCESS_Override(m, l, h) ACCESS_ENUM(m, l, h, Override)
#define ACCESS_SensorType(m, l, h) ACCESS_ENUM(m, l, h, SensorType)
#define ACCESS_Formula(m, l, h) ACCESS_ENUM(m, l, h, Formula)
#define ACCESS_JoystickMode(m, l, h) ACCESS_ENUM(m, l, h, JoystickMode)
#define ACCESS_JoystickChannel(m, l, h) ACCESS_ENUM(m, l, h, JoystickChannel)
#define ACCESS_Hats(m, l, h) ACCESS_ENUM(m, l, h, Hats)
bool physicalInputValue(PhysicalInputId& source, Field& field, const char* text)
{
  if (text) {
    auto& name = scalarBuffer;
    size_t length;
    if (!string(text, name, sizeof(name) - 1, length)) return false;
    name[length] = 0;
    if (!strcmp(name, "NONE")) { source = PhysicalInputId::None; return true; }
    const char* families[] = {"stick(%u)%n", "flex(%u)%n", "switch(%u)%n"};
    for (unsigned family = 0; family < 3; ++family) {
      unsigned index = 0;
      int used = 0;
      if (sscanf(name, families[family], &index, &used) == 1 && used &&
          !name[used] && index < 32) {
        // Keep portable hardware IDs even if this radio lacks that control.
        source = PhysicalInputId(1 + family * 32 + index);
        return true;
      }
    }
    return false;
  }
  unsigned id = unsigned(source);
  if (!id) { strcpy(field.value, "NONE"); return true; }
  if (id > 96) return false;
  const char* families[] = {"stick", "flex", "switch"};
  snprintf(field.value, sizeof(field.value), "%s(%u)", families[(id - 1) / 32], (id - 1) % 32);
  return true;
}
#define ACCESS_PhysicalInput(member, lo, hi) return physicalInputValue(m.member, f, t);

#define ACCESS_Source(member, lo, hi)                         \
  if (t) {                                                    \
    int64_t v;                                                \
    if (!sourceInput(t, v) || v < lo || v > hi) return false; \
    m.member = v;                                             \
    return true;                                              \
  }                                                           \
  {                                                           \
    char s[64];                                               \
    return sourceOutput(m.member, s, sizeof(s)) &&            \
           quoted(f.value, sizeof(f.value), s, strlen(s));    \
  }
#define ACCESS_Switch(member, lo, hi)                         \
  if (t) {                                                    \
    int64_t v;                                                \
    if (!switchInput(t, v) || v < lo || v > hi) return false; \
    m.member = v;                                             \
    return true;                                              \
  }                                                           \
  return switchOutput(m.member, f);
#define ACCESS_Sensor(member, lo, hi)            \
  if (t) {                                       \
    int64_t v;                                   \
    if (!strcmp(t, "none")) {                    \
      m.member = 0;                              \
      return true;                               \
    }                                            \
    if (!integer(t, 0, hi - 1, v)) return false; \
    m.member = v + 1;                            \
    return true;                                 \
  }                                              \
  if (!m.member) {                               \
    strcpy(f.value, "none");                     \
    return true;                                 \
  }                                              \
  return formatInteger(f.value, sizeof(f.value), m.member - 1);
#define ACCESS_ChannelCount(member, lo, hi)           \
  if (t) {                                            \
    int64_t v;                                        \
    if (!integer(t, lo + 8, hi + 8, v)) return false; \
    m.member = v - 8;                                 \
    return true;                                      \
  }                                                   \
  return formatInteger(f.value, sizeof(f.value), m.member + 8);
#define ACCESS_Throttle(member, lo, hi)                                   \
  if (t) {                                                                \
    int64_t v;                                                            \
    if (!sourceInput(t, v)) return false;                                 \
    int x = source2ThrottleSource(v);                                     \
    if (x < 0) return false;                                              \
    m.member = x;                                                         \
    return true;                                                          \
  }                                                                       \
  {                                                                       \
    char s[64];                                                           \
    return sourceOutput(throttleSource2Source(m.member), s, sizeof(s)) && \
           quoted(f.value, sizeof(f.value), s, strlen(s));                \
  }
#define ACCESS_ModelIds(member, lo, hi) return modelIds(m.member, f, t);
bool modelIds(uint8_t* ids, Field& f, const char* t)
{
  if (!t) {
    snprintf(f.value, sizeof(f.value), "[%u, %u]", ids[0], ids[1]);
    return true;
  }
  if (*t++ != '[') return false;
  uint8_t parsed[2] = {};
  unsigned count = 0;
  while (*t == ' ') ++t;
  while (*t && *t != ']') {
    if (count == 2) return false;
    const char* start = t;
    while (*t && *t != ',' && *t != ']') ++t;
    const char* end = t;
    while (end > start && end[-1] == ' ') --end;
    char number[16];
    size_t length = end - start;
    int64_t n;
    if (!length || length >= sizeof(number)) return false;
    memcpy(number, start, length);
    number[length] = 0;
    if (!integer(number, 0, 255, n)) return false;
    parsed[count++] = n;
    if (*t != ',') break;
    ++t;
    while (*t == ' ') ++t;
  }
  if (*t++ != ']' || *t) return false;
  ids[0] = parsed[0];
  ids[1] = parsed[1];
  return true;
}
struct Descriptor {
  const char* path;
  unsigned count, stride, innerStride;
  const char* codec;
  bool (*access)(ModelData&, unsigned, Field&, const char*);
};
#define ROW3(path, count, stride, inner, capability, member, lo, hi, codec)    \
  {path,                                                                       \
   count,                                                                      \
   stride,                                                                     \
   inner,                                                                      \
   #codec,                                                                     \
   [](ModelData& m, unsigned n, Field& f, const char* t) -> bool {             \
     unsigned i = n / (stride), j = (n % (stride)) / (inner), k = n % (inner); \
     (void)i;                                                                  \
     (void)j;                                                                  \
     (void)k;                                                                  \
     FUNCTION_ACCESS(path) f.available = (capability);                         \
     if (!f.available) return true;                                            \
     if (f.metadataOnly) return true;                                          \
     ACCESS_##codec(member, lo, hi)                                            \
   }},
#define ROW(p, n, stride, a, m, l, h, c) ROW3(p, n, stride, 1, a, m, l, h, c)
#define M(p, n, a, m, l, h, c) ROW(p, n, 1, a, m, l, h, c)
#define UNAVAILABLE(p, n, a, m, l, h, c) ROW(p, n, 1, false, view, 0, 0, Number)
#if LEN_BITMAP_NAME > 0
#define MBITMAP M
#else
#define MBITMAP UNAVAILABLE
#endif
#if defined(STORAGE_MODELSLIST)
#define MLABELS M
#else
#define MLABELS UNAVAILABLE
#endif
#if defined(COLORLCD)
#define MCOLOR M
#else
#define MCOLOR UNAVAILABLE
#endif
#if defined(USE_HATS_AS_KEYS)
#define MHATS M
#else
#define MHATS UNAVAILABLE
#endif
#if defined(PCBX9E)
#define MX9E M
#else
#define MX9E UNAVAILABLE
#endif
#if defined(PCBX9E) || defined(PCBX9DP)
#define MTOP M
#else
#define MTOP UNAVAILABLE
#endif
#define MSENSOR2(p, n, a, m, l, h, c) ROW(p, n, 4, a, m, l, h, c)
#if defined(COLORLCD)
#define MCOLOR10(p, n, a, m, l, h, c) ROW(p, n, 10, a, m, l, h, c)
#define MCOLOR500(p, n, a, m, l, h, c) ROW3(p, n, 500, 50, a, m, l, h, c)
#define MCOLOR50(p, n, a, m, l, h, c) ROW(p, n, 50, a, m, l, h, c)
#else
#define MCOLOR10(p, n, a, m, l, h, c) ROW(p, n, 10, false, view, 0, 0, Number)
#define MCOLOR500(p, n, a, m, l, h, c) \
  ROW3(p, n, 500, 50, false, view, 0, 0, Number)
#define MCOLOR50(p, n, a, m, l, h, c) ROW(p, n, 50, false, view, 0, 0, Number)
#endif
#define ACCESS_SwitchWarning(member, lo, hi)                               \
  if (t) {                                                                 \
    int64_t v;                                                             \
    if (!enumeration(SwitchWarning, t, v) || v < 0 || v > 3) return false; \
    m.setSwitchWarning(i, v);                                              \
    return true;                                                           \
  }                                                                        \
  return enumOutput(SwitchWarning, m.getSwitchWarning(i), f);
#define ACCESS_FunctionType(m, l, h) ACCESS_ENUM(m, l, h, FunctionType)
#define ACCESS_FunctionStart(m, l, h) ACCESS_ENUM(m, l, h, FunctionStart)
#define ACCESS_BooleanMode(m, l, h) ACCESS_ENUM(m, l, h, BooleanMode)
#define ACCESS_FunctionGroup(member, lo, hi) \
  if (t) {                                   \
    int64_t v;                               \
    if (!integer(t, 0, 1, v)) return false;  \
    m.cfsSetGroupAlwaysOn(i, v);             \
    return true;                             \
  }                                          \
  return formatInteger(f.value, sizeof(f.value), m.cfsGroupAlwaysOn(i));
#define ACCESS_FunctionId(member, lo, hi)                                \
  if (t) {                                                               \
    char b[32];                                                          \
    size_t len;                                                          \
    if (!string(t, b, sizeof(b) - 1, len)) return false;                 \
    b[len] = 0;                                                          \
    int idx = switchLookupIdx(b, len);                                   \
    return idx >= 0 && switchIsCustomSwitch(idx) &&                      \
           switchGetCustomSwitchIdx(idx) == i;                           \
  }                                                                      \
  for (unsigned x = 0; x < switchGetMaxSwitches(); ++x)                  \
    if (switchIsCustomSwitch(x) && switchGetCustomSwitchIdx(x) == i) {   \
      snprintf(f.value, sizeof(f.value), "%s", switchGetDefaultName(x)); \
      return true;                                                       \
    }                                                                    \
  return false;
#if defined(FUNCTION_SWITCHES)
#define MFUNCTION M
#else
#define MFUNCTION UNAVAILABLE
#endif
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
#define MFUNCTIONRGB M
#else
#define MFUNCTIONRGB UNAVAILABLE
#endif
constexpr Descriptor descriptors[] = {
#include "model_config_fields.inc"
};
constexpr unsigned DescriptorCount =
    sizeof(descriptors) / sizeof(descriptors[0]);
static unsigned bases[DescriptorCount + 1];
unsigned extent(const Descriptor& d)
{
#if defined(FUNCTION_SWITCHES)
  if (!strncmp(d.path, "customSwitches/", 15)) return d.count;
#endif
  auto& f = defaultField; f.reset();
  f.metadataOnly = true;
  d.access(candidate, 0, f, nullptr);
  return f.available ? d.count : 0;
}
void prepare()
{
  bases[0] = 0;
  for (unsigned i = 0; i < DescriptorCount; ++i)
    bases[i + 1] = bases[i] + extent(descriptors[i]);
}
bool set(void* ctx, unsigned row, unsigned id, const char* text)
{
  auto& c = *static_cast<Context*>(ctx);
  auto d = &descriptors[row];
  auto& f = defaultField; f.reset();
  if (!d) return false;
  unsigned i = id / d->stride, j = id % d->stride;
  if (!d->access(*c.model, id, f, text)) {
    TRACE("model configuration: invalid %s: %s", d->path, text);
    return false;
  }
  if (c.loading && !strncmp(d->path, "telemetrySensors/", 17) &&
      i < MAX_TELEMETRY_SENSORS) {
    int slot = sensorSlot(d->path, j);
    if (slot >= 0) {
      int64_t v;
      if (slot == 3) {
        if (!enumeration(Formula, text, v)) return false;
      } else if (!integer(text, INT32_MIN, UINT32_MAX, v))
        return false;
      sensorStage[i].values[slot] = v;
    }
  }
  return true;
}
bool portableUnavailable(const char* codec, const char* text)
{
  if (!strcmp(codec, "Module")) {
    // Keep the previous model loader's RF safety rule: only these two
    // symbolic names select a protocol. Numeric and unknown types stay off.
    // Empty legacy RF selections also mean off, including an unquoted null.
    if (!*text) return true;
    if (strchr("[{|>", *text)) return false;
    auto& name = scalarBuffer;
    size_t length;
    if (!string(text, name, sizeof(name) - 1, length)) return false;
    name[length] = 0;
    return strcmp(name, "TYPE_NONE") && strcmp(name, "TYPE_CROSSFIRE");
  }
  if (!strstr(codec, "Source") && strcmp(codec, "Switch") &&
      strcmp(codec, "Throttle"))
    return false;
  int64_t n;
  if (!strcmp(codec, "Switch") ? switchInput(text, n) : sourceInput(text, n))
    return false;
  auto& s = scalarBuffer;
  size_t len;
  if (!string(text, s, sizeof(s) - 1, len)) return false;
  s[len] = 0;
  const char* p = s;
  if (*p == '!') ++p;
  int a = 0, used = 0;
  if (sscanf(p, "tele(%d)%n", &a, &used) == 1 && used && !p[used] &&
      abs(a) < 99)
    return true;
  const char* analogNames[] = {"LH",  "LV",   "RH",   "RV",   "JSx",
                               "JSy", "EXT1", "EXT2", "EXT3", "EXT4",
                               "SL1", "SL2",  "SL3",  "SL4"};
  for (auto name : analogNames)
    if (!strcmp(p, name)) return true;
  used = 0;
  if (sscanf(p, "GR%d%n", &a, &used) == 1 && !p[used] && a > 0 && a <= 8)
    return true;
  if (!strcmp(codec, "Switch")) {
    used = 0;
    if (sscanf(p, "T%d%n", &a, &used) == 1 && !p[used] && a > 0 && a <= 99)
      return true;
    if (!strncmp(p, "TrimT", 5) && p[5] >= '1' && p[5] <= '8' &&
        (!strcmp(p + 6, "Up") || !strcmp(p + 6, "Down")))
      return true;
  }
  // Canonical hardware names are known even when the device has no instance.
  if ((*p == 'S' && p[1] >= 'A' && p[1] <= 'Z') || !strncmp(p, "SW", 2) ||
      !strncmp(p, "FL", 2))
    return strlen(p) <= 5;
  if ((*p == 'P' || *p == 'A') && p[1] >= '0' && p[1] <= '9')
    return strlen(p) <= 3;
  return false;
}
// Apply one leaf selected by its enclosing section. No path construction or search.
void readLeaf(void* ctx, unsigned row, unsigned local, const char* text, Result& result)
{
  const auto& d = descriptors[row];
  if (local >= bases[row + 1] - bases[row]) return;
  unsigned id = bases[row] + local;
  if (!mark(seen, id, sizeof(seen), result)) return;
  auto& f = defaultField; f.reset(); f.metadataOnly = true;
  if (!d.access(*static_cast<Context*>(ctx)->model, local, f, nullptr) || !f.available) return;
  if (!strcmp(d.codec, "ModelIds")) {
    for (unsigned n = 0; n < 2; ++n) if (!mark(seen, bases[3] + n, sizeof(seen), result)) return;
  }
  int64_t reference;
  if (!strcmp(d.codec, "Sensor") && integer(text, 0, 98, reference) && reference >= MAX_TELEMETRY_SENSORS) return;
  if (!strncmp(d.path, "telemetrySensors/", 17) &&
      (strstr(d.path, "/cfg/cell/source") || strstr(d.path, "/cfg/consumption/source") ||
       strstr(d.path, "/cfg/calc/sources/") || strstr(d.path, "/cfg/dist/")) &&
      integer(text, -99, 99, reference) && (reference > MAX_TELEMETRY_SENSORS || reference < -MAX_TELEMETRY_SENSORS)) return;
  if (portableUnavailable(d.codec, text)) return;
  if (!set(ctx, row, local, text)) ++result.invalid;
}
// Drop empty runtime slots, even if their inactive union contains old data.
bool used(const Context& c, const char* section, const unsigned* index)
{
  auto& m = *c.model; unsigned i = index[0];
#if defined(COLORLCD)
  unsigned j = index[1], k = index[2];
#endif
#if defined(FUNCTION_SWITCHES)
  if (!strcmp(section, "customSwitches/%u")) {
    int native = functionIndex(*c.model, i);
    if (native < 0 || native >= NUM_FUNCTIONS_SWITCHES) return false;
    const auto& a = m.customSwitches[native];
    const auto& b = defaults.customSwitches[native];
    return strncmp(a.name, b.name, LEN_SWITCH_NAME) || a.type != b.type ||
           a.group != b.group || a.start != b.start || a.state != b.state
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
           || a.onColorLuaOverride != b.onColorLuaOverride ||
           a.offColorLuaOverride != b.offColorLuaOverride ||
           a.onColor.r != b.onColor.r || a.onColor.g != b.onColor.g || a.onColor.b != b.onColor.b ||
           a.offColor.r != b.offColor.r || a.offColor.g != b.offColor.g || a.offColor.b != b.offColor.b
#endif
           ;
  }
#endif
  if (!strcmp(section, "channelMappings/%u")) return i < MAX_OUTPUT_CHANNELS && m.channelMappings[i].source != PhysicalInputId::None;
  if (!strcmp(section, "telemetrySensors/%u")) return i < MAX_TELEMETRY_SENSORS && (m.telemetrySensors[i].id || m.telemetrySensors[i].label[0] || m.telemetrySensors[i].type);
#if !defined(COLORLCD)
  if (!strncmp(section, "screenData/", 11) || !strncmp(section, "topbarData/", 11) || !strncmp(section, "topbarWidgetWidth/", 18)) return false;
#else
  if (!strncmp(section, "topbarWidgetWidth/", 18)) return i < MAX_TOPBAR_ZONES && !topbarFor(*c.model).zones[i].widgetName.empty();
  if (!strncmp(section, "screenData/", 11)) {
    if (i >= MAX_CUSTOM_SCREENS || (&m == &candidate ? stagedScreens[i].LayoutId.empty() : (!m.hasScreenData(i) || m.getScreenData(i)->LayoutId.empty()))) return false;
    if (strstr(section, "/zones/")) {
      if (j >= MAX_LAYOUT_ZONES || screenFor(*c.model, i).layoutData.zones[j].widgetName.empty()) return false;
      if (strstr(section, "/options/")) return k < screenFor(*c.model, i).layoutData.zones[j].widgetData.options.size();
    }
  }
  if (!strncmp(section, "topbarData/", 11)) {
    if (i >= MAX_TOPBAR_ZONES || topbarFor(*c.model).zones[i].widgetName.empty()) return false;
    if (strstr(section, "/options/")) return j < topbarFor(*c.model).zones[i].widgetData.options.size();
  }
#endif
  if (!strcmp(section, "switchWarning/%u")) return i < switchGetMaxAllSwitches();
  return true;
}
void writeLeaf(Context& context, unsigned row, unsigned local, const char* key, Writer& writer, Field& f)
{
  auto& d = descriptors[row];
  if (local >= bases[row + 1] - bases[row]) return;
  unsigned i = local / d.stride, j = (local % d.stride) / d.innerStride, k = local % d.innerStride;
  if (!relevant(context, d.path, i, j, k)) return;
  f.reset();
  if (!d.access(*context.model, local, f, nullptr)) { writer.error = "invalid runtime model value"; return; }
  if (!f.available) return;
  defaultField.reset();
  if (!strncmp(d.path, "screenData/", 11) || !strncmp(d.path, "topbarData/", 11)) {
    const char* baseline = "0";
    if (strstr(d.codec, "Name") || strstr(d.codec, "String") || !strcmp(d.codec, "LayoutId")) baseline = "\"\"";
    else if (!strcmp(d.codec, "LayoutOptionType")) baseline = "None";
    else if (strstr(d.codec, "Color")) baseline = "COLIDX0";
    else if (strstr(d.codec, "Source")) baseline = "\"NONE\"";
    strcpy(defaultField.value, baseline);
  } else if (!d.access(defaults, local, defaultField, nullptr)) { writer.error = "invalid model default"; return; }
  // A widget option's type is its presence marker, including a zero-valued option.
  bool presence = strstr(d.codec, "WidgetType") || !strcmp(d.codec, "WidgetType") || !strcmp(d.codec, "FunctionId");
  if (presence || strcmp(f.value, defaultField.value)) writer.value(key, f.value);
}
constexpr bool sameKey(const char* a, const char* b) {
  while (*a && *a == *b) { ++a; ++b; }
  return *a == *b;
}
#include "model_config_sections.inc"
void save(void* ctx, Writer& writer, Field& field)
{
  initialize(defaults);
  auto& context = *static_cast<Context*>(ctx);
  unsigned index[3]{};
  writeSection(context, 0, index, writer, field);
}
Document make(Context& c)
{
  prepare();
  return {&c, enter, save, seen, sizeof(seen)};
}
void initialize(ModelData& model)
{
  model = ModelData{};
  model.rfAlarms.warning = 45;
  model.rfAlarms.critical = 42;
#if defined(FUNCTION_SWITCHES)
  for (unsigned x = 0; x < switchGetMaxSwitches(); ++x)
    if (switchIsCustomSwitch(x)) {
      unsigned i = switchGetCustomSwitchIdx(x);
      auto& sw = model.customSwitches[i];
      const char* name = switchGetDefaultName(x);
      bool grouped = !strncmp(name, "SW", 2);
      sw.type = grouped ? SWITCH_2POS : SWITCH_GLOBAL;
      sw.group = grouped ? 1 : 0;
      sw.start = grouped ? (!strcmp(name, "SW1") ? FS_START_ON : FS_START_OFF)
                         : FS_START_PREVIOUS;
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
      sw.onColor.setColor(0xFFFFFF);
#endif
    }
  model.cfsSetGroupAlwaysOn(1, true);
#endif
}
}  // namespace
ModelData& loadedCandidate() { return candidate; }
config_stream::Document document(ModelData& model)
{
  saveContext = {&model, false};
  return make(saveContext);
}
config_stream::Document beginLoad()
{
  initialize(candidate);
#if defined(FUNCTION_SWITCHES)
  beginFunctions();
#endif
  memset(sensorStage, 0, sizeof(sensorStage));
#if defined(COLORLCD)
  for (auto& screen : stagedScreens) {
    screen.LayoutId.clear();
    screen.layoutData.clear();
  }
  stagedTopbar.clear();
  clearScreenStages();
#endif
  return make(loadContext);
}
const char* resolveLoad()
{
#if defined(FUNCTION_SWITCHES)
  if (const char* error = resolveFunctions()) return error;
#endif
#if defined(COLORLCD)
  resolveScreenStages();
#endif
  for (unsigned i = 0; i < MAX_TELEMETRY_SENSORS; ++i) {
    auto& s = candidate.telemetrySensors[i];
    auto& v = sensorStage[i].values;
    s.id = (s.type == TELEM_TYPE_CALCULATED && s.persistent) ? v[1] : v[0];
    s.instance = s.type == TELEM_TYPE_CALCULATED ? v[3] : v[2];
    s.param = 0;
    if (s.unit >= UNIT_FIRST_VIRTUAL)
      s.param = v[11];
    else if (s.type != TELEM_TYPE_CALCULATED) {
      s.custom.ratio = v[4];
      s.custom.offset = v[5];
    } else if (s.formula == TELEM_FORMULA_CELL) {
      s.cell.source = v[6];
      s.cell.index = v[7];
    } else if (s.formula == TELEM_FORMULA_DIST) {
      s.dist.gps = v[9];
      s.dist.alt = v[10];
    } else if (s.formula == TELEM_FORMULA_CONSUMPTION ||
               s.formula == TELEM_FORMULA_TOTALIZE)
      s.consumption.source = v[8];
    else
      for (unsigned j = 0; j < 4; ++j) s.calc.sources[j] = v[12 + j];
  }
  return nullptr;
}
void commitLoad(ModelData& model)
{
#if defined(COLORLCD)
  model.resetScreenData();
  for (unsigned i = 0; i < MAX_CUSTOM_SCREENS; ++i)
    if (!stagedScreens[i].LayoutId.empty())
      std::swap(*model.getScreenData(i), stagedScreens[i]);
  std::swap(*model.getTopbarData(), stagedTopbar);
#endif
  model = candidate;
#if defined(FUNCTION_SWITCHES)
  memcpy(functionSlots, pendingFunctionSlots, sizeof(functionSlots));
#endif
}
namespace
{
bool headerSet(void* ctx, unsigned id, const char* text)
{
  auto& h = *static_cast<Header*>(ctx);
  auto& f = defaultField; f.reset();
  if (id == 0) return textValue(h.header.name, f, text);
  if (id == 1) return modelIds(h.header.modelId, f, text);
#if LEN_BITMAP_NAME > 0
  if (id == 2) return textValue(h.header.bitmap, f, text);
#endif
#if defined(STORAGE_MODELSLIST)
  if (id == 3) return textValue(h.header.labels, f, text);
#endif
  int64_t n;
  if (id >= 6) {
    if (!integer(text, 0, 255, n)) return false;
    h.header.modelId[id - 6] = n;
    return true;
  }
  if (id >= 4 && portableUnavailable("Module", text)) {
    h.moduleData[id - 4].type = MODULE_TYPE_NONE;
    return true;
  }
  if (id >= 4 && enumeration(Module, text, n) &&
      (n == MODULE_TYPE_NONE || n == MODULE_TYPE_CROSSFIRE)) {
    h.moduleData[id - 4].type = n;
    return true;
  }
  return false;
}
void headerEnter(void* ctx, const Node& parent, const char* key, const char* text, Node& child, Result& result)
{
  child = parent; child.section = -1;
  int id = -1; int64_t i;
  switch (parent.section) {
    case 0:
      if (!strcmp(key, "header")) child.section = 1;
      if (!strcmp(key, "moduleData")) child.section = 2;
      break;
    case 1:
      if (!strcmp(key, "name")) id = 0;
      if (!strcmp(key, "modelId")) { child.section = 3; if (*text) id = 1; }
      if (!strcmp(key, "bitmap")) id = 2;
      if (!strcmp(key, "labels")) id = 3;
      break;
    case 2:
      if (integer(key, 0, 1, i)) { child.section = 4; child.index[0] = i; }
      break;
    case 3:
      if (integer(key, 0, 1, i)) {
        child.section = 5; child.index[0] = i;
        if (*text) id = 6 + i;
      }
      break;
    case 4: if (!strcmp(key, "type")) id = 4 + parent.index[0]; break;
    case 5: if (!strcmp(key, "val")) id = 6 + parent.index[0]; break;
  }
#if LEN_BITMAP_NAME == 0
  if (id == 2) return;
#endif
#if !defined(STORAGE_MODELSLIST)
  if (id == 3) return;
#endif
  if (id >= 0 && *text) {
    if (!mark(seen, id, sizeof(seen), result)) return;
    if (id == 1 && (!mark(seen, 6, sizeof(seen), result) || !mark(seen, 7, sizeof(seen), result))) return;
    if (!headerSet(ctx, id, text)) ++result.invalid;
  } else if (child.section >= 0 && *text) result.error = "expected configuration mapping";
}
} // namespace
config_stream::Document headerDocument(Header& h)
{
  return {&h, headerEnter, nullptr, seen, sizeof(seen)};
}
} // namespace model_config
