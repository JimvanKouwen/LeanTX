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
const Enum Multiplex[] = {{"ADD", 0}, {"MUL", 1}, {"REPL", 2}, {nullptr, 0}};
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
const Enum ScreenType[] = {
    {"NONE", 0}, {"VALUES", 1}, {"BARS", 2}, {"SCRIPT", 3}, {nullptr, 0}};
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
  char decoded[MaxValue];
  size_t len;
  if (!string(text, decoded, sizeof(decoded), len)) return false;
  memset(value, 0, N);
  memcpy(value, decoded, len < N ? len : N);
  return true;
}
bool enumeration(const Enum* table, const char* text, int64_t& n)
{
  char decoded[MaxValue];
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
  if (n >= MIXSRC_FIRST_INPUT && n <= MIXSRC_LAST_INPUT)
    snprintf(out, cap, "I%d", n - MIXSRC_FIRST_INPUT);
#if defined(LUA_INPUTS)
  else if (n >= MIXSRC_FIRST_LUA && n <= MIXSRC_LAST_LUA)
    snprintf(out, cap, "lua(%d,%d)",
             (n - MIXSRC_FIRST_LUA) / MAX_SCRIPT_OUTPUTS,
             (n - MIXSRC_FIRST_LUA) % MAX_SCRIPT_OUTPUTS);
#endif
  else if (n >= MIXSRC_FIRST_STICK && n <= MIXSRC_LAST_STICK)
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
  char s[MaxValue];
  size_t len;
  if (!string(text, s, sizeof(s) - 1, len)) return false;
  s[len] = 0;
  const char* p = s;
  int sign = 1;
  if (*p == '!') {
    sign = -1;
    ++p;
  }
  int n = -1, a = 0, b = 0, used = 0;
  for (auto e = sources; e->name; ++e)
    if (!strcmp(p, e->name)) n = e->value;
  if (n < 0 && sscanf(p, "I%d%n", &a, &used) == 1 && !p[used] && a >= 0 &&
      a < MAX_INPUTS)
    n = MIXSRC_FIRST_INPUT + a;
  used = 0;
#if defined(LUA_INPUTS)
  if (n < 0 && sscanf(p, "lua(%d,%d)%n", &a, &b, &used) == 2 && used &&
      !p[used] && a >= 0 && a < MAX_SCRIPTS && b >= 0 && b < MAX_SCRIPT_OUTPUTS)
    n = MIXSRC_FIRST_LUA + a * MAX_SCRIPT_OUTPUTS + b;
#endif
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
  char s[MaxValue];
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
static ModelData candidate;
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
#if !defined(COLORLCD)
static TelemetryScreenData monoStage[4][3];
#endif
int screenVariant(const char* path)
{
  return strstr(path, "/u/bars/")     ? 0
         : strstr(path, "/u/lines/")  ? 1
         : strstr(path, "/u/script/") ? 2
                                      : -1;
}
static bool scriptSource[9][6];
static bool candidateScriptSource[9][6];
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
#if !defined(COLORLCD)
  if (!strncmp(path, "screens/", 8)) {
    int variant = screenVariant(path);
    unsigned type = c.model->getTelemetryScreenType(i);
    if (variant >= 0) return variant == int(type == 1 ? 1 : type == 3 ? 2 : 0);
  }
#endif
  if (strstr(path, "scriptsData/") && strstr(path, "/inputs/")) {
    bool source = (c.loading ? candidateScriptSource : scriptSource)[i][j];
    return (strstr(path, "/source") != nullptr) == source;
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
#define ACCESS_Multiplex(m, l, h) ACCESS_ENUM(m, l, h, Multiplex)
#define ACCESS_Module(m, l, h) ACCESS_ENUM(m, l, h, Module)
#define ACCESS_Antenna(m, l, h) ACCESS_ENUM(m, l, h, Antenna)
#define ACCESS_PotsMode(m, l, h) ACCESS_ENUM(m, l, h, PotsMode)
#define ACCESS_Override(m, l, h) ACCESS_ENUM(m, l, h, Override)
#define ACCESS_SensorType(m, l, h) ACCESS_ENUM(m, l, h, SensorType)
#define ACCESS_Formula(m, l, h) ACCESS_ENUM(m, l, h, Formula)
#define ACCESS_JoystickMode(m, l, h) ACCESS_ENUM(m, l, h, JoystickMode)
#define ACCESS_JoystickChannel(m, l, h) ACCESS_ENUM(m, l, h, JoystickChannel)
#define ACCESS_Hats(m, l, h) ACCESS_ENUM(m, l, h, Hats)
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
#define CAP_Antenna(member) 0
#define CAP_BooleanMode(member) 0
#define CAP_ChannelCount(member) 0
#define CAP_Formula(member) 0
#define CAP_FunctionGroup(member) 0
#define CAP_FunctionId(member) 0
#define CAP_FunctionStart(member) 0
#define CAP_FunctionType(member) 0
#define CAP_Hats(member) 0
#define CAP_JoystickChannel(member) 0
#define CAP_JoystickMode(member) 0
#define CAP_LayoutBool(member) 0
#define CAP_LayoutColor(member) 0
#define CAP_LayoutId(member) 0
#define CAP_LayoutOptionType(member) 0
#define CAP_LayoutUnsigned(member) 0
#define CAP_ModelIds(member) 0
#define CAP_Module(member) 0
#define CAP_Multiplex(member) 0
#define CAP_Number(member) 0
#define CAP_Override(member) 0
#define CAP_PotsMode(member) 0
#define CAP_ScreenType(member) 0
#define CAP_Sensor(member) 0
#define CAP_SensorType(member) 0
#define CAP_Source(member) 0
#define CAP_Switch(member) 0
#define CAP_SwitchWarning(member) 0
#define CAP_Text(member) sizeof(m.member)
#define CAP_Throttle(member) 0
#define CAP_TimerMode(member) 0
#define CAP_TopbarName(member) 0
#define CAP_TopbarWidgetBool(member) 0
#define CAP_TopbarWidgetColor(member) 0
#define CAP_TopbarWidgetSigned(member) 0
#define CAP_TopbarWidgetSource(member) 0
#define CAP_TopbarWidgetString(member) 0
#define CAP_TopbarWidgetType(member) 0
#define CAP_TopbarWidgetUnsigned(member) 0
#define CAP_WidgetBool(member) 0
#define CAP_WidgetColor(member) 0
#define CAP_WidgetName(member) 0
#define CAP_WidgetSigned(member) 0
#define CAP_WidgetSource(member) 0
#define CAP_WidgetString(member) 0
#define CAP_WidgetType(member) 0
#define CAP_WidgetUnsigned(member) 0
#define CAP_codec(member) 0
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
     f.capacity = CAP_##codec(member);                                         \
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
#if MAX_SCRIPTS > 0
#define MSCRIPT M
#define MSCRIPT2(p, n, a, m, l, h, c) ROW(p, n, 6, a, m, l, h, c)
#else
#define MSCRIPT UNAVAILABLE
#define MSCRIPT2(p, n, a, m, l, h, c) ROW(p, n, 6, false, view, 0, 0, Number)
#endif
#define MSENSOR2(p, n, a, m, l, h, c) ROW(p, n, 4, a, m, l, h, c)
#define ACCESS_ScreenType(member, lo, hi)                               \
  if (t) {                                                              \
    int64_t v;                                                          \
    if (!enumeration(ScreenType, t, v) || v < 0 || v > 3) return false; \
    m.setTelemetryScreenType(i, v);                                     \
    return true;                                                        \
  }                                                                     \
  return enumOutput(ScreenType, m.getTelemetryScreenType(i), f);
#if !defined(COLORLCD)
#define MMONO M
#define MMONO4(p, n, a, m, l, h, c) ROW(p, n, 4, a, m, l, h, c)
#define MMONO8(p, n, a, m, l, h, c) ROW(p, n, 8, a, m, l, h, c)
#define MMONOLINES(p, n, a, m, l, h, c) ROW3(p, n, 12, 3, a, m, l, h, c)
#else
#define MMONO UNAVAILABLE
#define MMONO4(p, n, a, m, l, h, c) ROW(p, n, 4, false, view, 0, 0, Number)
#define MMONO8(p, n, a, m, l, h, c) ROW(p, n, 8, false, view, 0, 0, Number)
#define MMONOLINES(p, n, a, m, l, h, c) \
  ROW3(p, n, 12, 3, false, view, 0, 0, Number)
#endif
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
const Descriptor descriptors[] = {
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
  Field f{};
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
unsigned count() { return bases[DescriptorCount]; }
unsigned rowFor(unsigned id)
{
  unsigned lo = 0, hi = DescriptorCount;
  while (lo < hi) {
    unsigned mid = (lo + hi) / 2;
    if (bases[mid + 1] <= id)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo;
}
const Descriptor* descriptor(unsigned& id)
{
  unsigned row = rowFor(id);
  if (row == DescriptorCount) return nullptr;
  id -= bases[row];
  return &descriptors[row];
}
unsigned next(void*, const char* prefix, unsigned begin)
{
  char translated[MaxPath];
  if (!strncmp(prefix, "switchWarning/", 14)) {
    const char* end = strchr(prefix + 14, '/');
    size_t len = end ? size_t(end - prefix - 14) : strlen(prefix + 14);
    if (len < 32) {
      char name[32];
      memcpy(name, prefix + 14, len);
      name[len] = 0;
      int i = switchLookupIdx(name, len);
      if (i >= 0) {
        snprintf(translated, sizeof(translated), "switchWarning/%d%s", i,
                 end ? end : "");
        prefix = translated;
      }
    }
  }
  for (unsigned row = rowFor(begin); row < DescriptorCount; ++row) {
    auto& d = descriptors[row];
    unsigned first = 0, last = d.count;
    const char* p = prefix;
    const char* q = d.path;
    unsigned dimension = 0;
    bool match = true;
    while (*p && *q) {
      if (q[0] == '%' && q[1] == 'u') {
        if (*p < '0' || *p > '9') {
          match = false;
          break;
        }
        unsigned value = 0;
        while (*p >= '0' && *p <= '9') {
          value = value * 10 + *p++ - '0';
          if (value > 65535) {
            match = false;
            break;
          }
        }
        unsigned stride = dimension == 0   ? d.stride
                          : dimension == 1 ? d.innerStride
                                           : 1;
        if (value >= (dimension == 0   ? d.count / d.stride
                      : dimension == 1 ? d.stride / d.innerStride
                                       : d.innerStride)) {
          match = false;
          break;
        }
        first += value * stride;
        last = first + stride;
        ++dimension;
        q += 2;
      } else {
        if (*p != *q) {
          match = false;
          break;
        }
        ++p;
        ++q;
      }
    }
    if (!match || *p || (*prefix && *q && *q != '/')) continue;
    unsigned id = begin > bases[row] + first ? begin : bases[row] + first;
    if (id < bases[row + 1] && id < bases[row] + last) return id;
  }
  return count();
}
bool describe(void* ctx, unsigned id, Field& f)
{
  auto& c = *static_cast<Context*>(ctx);
  auto d = descriptor(id);
  if (!d) return false;
  snprintf(f.path, sizeof(f.path), d->path, id / d->stride,
           (id % d->stride) / d->innerStride, id % d->innerStride);
  if (!strcmp(d->path, "switchWarning/%u/pos") &&
      id < switchGetMaxAllSwitches())
    snprintf(f.path, sizeof(f.path), "switchWarning/%s/pos",
             switchGetDefaultName(id));
  f.metadataOnly = true;
  bool ok = d->access(*c.model, id, f, nullptr);
  f.metadataOnly = false;
  f.required = f.available &&
               relevant(c, d->path, id / d->stride,
                        (id % d->stride) / d->innerStride, id % d->innerStride);
  if (!c.loading && strcmp(d->path, "header/modelId"))
    f.available = f.available && f.required;
  return ok;
}
bool field(void* ctx, unsigned id, Field& f)
{
  if (!describe(ctx, id, f)) return false;
  auto& c = *static_cast<Context*>(ctx);
  auto d = descriptor(id);
  if (!f.available) return true;
  return d->access(*c.model, id, f, nullptr);
}
bool set(void* ctx, unsigned id, const char* text)
{
  auto& c = *static_cast<Context*>(ctx);
  auto d = descriptor(id);
  Field f{};
  if (!d) return false;
  unsigned i = id / d->stride, j = id % d->stride;
#if !defined(COLORLCD)
  int variant = !strncmp(d->path, "screens/", 8) ? screenVariant(d->path) : -1;
  if (c.loading && variant >= 0 && i < 4)
    c.model->screens[i] = monoStage[i][variant];
#endif
  if (!d->access(*c.model, id, f, text)) {
    TRACE("model configuration: invalid %s: %s", d->path, text);
    return false;
  }
#if !defined(COLORLCD)
  if (c.loading && variant >= 0 && i < 4)
    monoStage[i][variant] = c.model->screens[i];
#endif
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
  if (c.loading && strstr(d->path, "scriptsData/") &&
      strstr(d->path, "/inputs/") && i < 9 && j < 6)
    candidateScriptSource[i][j] = strstr(d->path, "/source") != nullptr;
  return true;
}
int resolve(void*, const char* path)
{
  char translated[MaxPath];
  if (!strncmp(path, "switchWarning/", 14)) {
    const char* end = strchr(path + 14, '/');
    if (end) {
      char name[32];
      size_t len = end - path - 14;
      if (len < sizeof(name)) {
        memcpy(name, path + 14, len);
        name[len] = 0;
        int i = switchLookupIdx(name, len);
        if (i >= 0) {
          snprintf(translated, sizeof(translated), "switchWarning/%d%s", i,
                   end);
          path = translated;
        }
      }
    }
  }
  unsigned base = 0;
  for (auto& d : descriptors) {
    const char* p = path;
    const char* pattern = d.path;
    unsigned indices[3] = {}, index = 0;
    while (*pattern && *p) {
      if (pattern[0] == '%' && pattern[1] == 'u') {
        if (index == 3 || *p < '0' || *p > '9') break;
        unsigned n = 0;
        while (*p >= '0' && *p <= '9') {
          n = n * 10 + *p++ - '0';
          if (n > 65535) return -1;
        }
        indices[index++] = n;
        pattern += 2;
      } else {
        if (*pattern != *p) break;
        ++pattern;
        ++p;
      }
    }
    if ((!*pattern || !strcmp(pattern, "/val")) && !*p) {
      unsigned n =
          indices[0] * d.stride + indices[1] * d.innerStride + indices[2];
      if (n < extent(d) &&
          (d.stride == 1 || indices[1] * d.innerStride < d.stride) &&
          indices[2] < d.innerStride)
        return base + n;
    }
    base += extent(d);
  }
  return -1;
}
bool known(void* ctx, const char* path)
{
  if (resolve(ctx, path) >= 0) return true;
  if (!strncmp(path, "switchWarning/", 14)) {
    const char* p = strchr(path + 14, '/');
    return !p || !strcmp(p, "/pos");
  }
  // Match structural prefixes against the same universal schema.
  for (auto& d : descriptors) {
    const char* p = path;
    const char* q = d.path;
    while (*p && *q) {
      if (q[0] == '%' && q[1] == 'u') {
        if (*p < '0' || *p > '9') break;
        while (*p >= '0' && *p <= '9') ++p;
        q += 2;
      } else {
        if (*p != *q) break;
        ++p;
        ++q;
      }
    }
    if (!*p && (*q == '/' || !*q)) return true;
  }
  return !strcmp(path, "semver") || !strcmp(path, "checksum");
}
bool sequence(void*, const char* path)
{
  if (!strcmp(path, "switchWarning")) return false;
  for (auto& d : descriptors) {
    const char* p = path;
    const char* q = d.path;
    while (*p && *q) {
      if (q[0] == '%' && q[1] == 'u') {
        if (*p < '0' || *p > '9') break;
        while (*p >= '0' && *p <= '9') ++p;
        q += 2;
      } else {
        if (*p != *q) break;
        ++p;
        ++q;
      }
    }
    if (!*p && !strncmp(q, "/%u/", 4)) return true;
  }
  return false;
}
bool portableUnavailable(const char* codec, const char* text)
{
  if (!strcmp(codec, "Module")) {
    // Keep the previous model loader's RF safety rule: only these two
    // symbolic names select a protocol. Numeric and unknown types stay off.
    // Empty legacy RF selections also mean off, including an unquoted null.
    if (!*text) return true;
    if (strchr("[{|>", *text)) return false;
    char name[MaxValue];
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
  char s[MaxValue];
  size_t len;
  if (!string(text, s, sizeof(s) - 1, len)) return false;
  s[len] = 0;
  const char* p = s;
  if (*p == '!') ++p;
  int a = 0, b = 0, used = 0;
  if (sscanf(p, "lua(%d,%d)%n", &a, &b, &used) == 2 && used && !p[used] &&
      a >= 0 && a < 9 && b >= 0 && b < MAX_SCRIPT_OUTPUTS)
    return true;
  used = 0;
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
// Serialized with the file transaction, so large quoted strings do not consume
// the menus-task stack while checking whether a narrower target changed them.
static struct {
  char token[MaxLine];
  Field field;
} scalarScratch;
bool preserve(void* ctx, unsigned id, const char* text)
{
  // The engine retains the original line for output; inspect an independent
  // scalar token so an inline comment cannot hide an unavailable value.
  auto& token = scalarScratch.token;
  if (strchr(text, '#')) {
    size_t length = strlen(text);
    if (length >= sizeof(token)) return false;
    memcpy(token, text, length + 1);
    char quote = 0;
    for (char* p = token; *p; ++p) {
      if (quote) {
        if (quote == '"' && *p == '\\' && p[1])
          ++p;
        else if (*p == quote) {
          if (quote == '\'' && p[1] == '\'')
            ++p;
          else
            quote = 0;
        }
      } else if (*p == '\'' || *p == '"')
        quote = *p;
      else if (*p == '#' && (p == token || p[-1] == ' ')) {
        *p = 0;
        break;
      }
    }
    length = strlen(token);
    while (length && token[length - 1] == ' ') token[--length] = 0;
    text = token;
  }
  unsigned local = id;
  auto d = descriptor(local);
  if (!d) return false;
  // Preserve unavailable values during ordinary saves, but allow a user to
  // replace the fallback with an available setting on this radio.
  auto unchangedFallback = [&](const char* codec, int baseline = 0) {
    auto& context = *static_cast<Context*>(ctx);
    if (context.loading) return true;
    auto& current = scalarScratch.field;
    current = Field{};
    if (!d->access(*context.model, local, current, nullptr)) return false;
    int64_t value;
    if (!strcmp(codec, "Sensor")) return !strcmp(current.value, "none");
    if (!strcmp(codec, "Module"))
      return enumeration(Module, current.value, value) && value == baseline;
    if (!strcmp(codec, "Switch"))
      return switchInput(current.value, value) && value == baseline;
    if (!strcmp(codec, "Throttle"))
      return sourceInput(current.value, value) &&
             source2ThrottleSource(value) == baseline;
    if (strstr(codec, "Source"))
      return sourceInput(current.value, value) && value == baseline;
    return integer(current.value, INT32_MIN, UINT32_MAX, value) &&
           value == baseline;
  };
  if (!strcmp(d->codec, "ModelIds") && (!*text || *text == '#')) return true;
#if defined(FUNCTION_SWITCHES)
  if (!strcmp(d->path, "customSwitches/%u/group")) {
    int64_t group;
    if (integer(text, 0, 7, group) && group > NUM_FUNCTIONS_GROUPS) {
      int native = functionIndex(*static_cast<Context*>(ctx)->model, local);
      const char* name =
          native >= 0
              ? switchGetDefaultName(switchGetSwitchFromCustomIdx(native))
              : "";
      return unchangedFallback("Number",
                               name && !strncmp(name, "SW", 2) ? 1 : 0);
    }
  }
#endif
  if (!strcmp(d->codec, "Sensor")) {
    int64_t sensor;
    if (integer(text, 0, 98, sensor) && sensor >= MAX_TELEMETRY_SENSORS)
      return unchangedFallback("Sensor");
  }
  if (!strncmp(d->path, "telemetrySensors/", 17) &&
      (strstr(d->path, "/cfg/cell/source") ||
       strstr(d->path, "/cfg/consumption/source") ||
       strstr(d->path, "/cfg/calc/sources/") ||
       strstr(d->path, "/cfg/dist/"))) {
    int64_t sensor;
    if (integer(text, -99, 99, sensor) &&
        (sensor > MAX_TELEMETRY_SENSORS || sensor < -MAX_TELEMETRY_SENSORS))
      return unchangedFallback("Number");
  }
  if (portableUnavailable(d->codec, text)) {
    if (static_cast<Context*>(ctx)->loading &&
        !strncmp(d->path, "scriptsData/", 12) && strstr(d->path, "/inputs/") &&
        strstr(d->path, "/source"))
      candidateScriptSource[local / d->stride][local % d->stride] = true;
    return unchangedFallback(d->codec);
  }
  if (!static_cast<Context*>(ctx)->loading && !strcmp(d->codec, "Text")) {
    auto& f = scalarScratch.field;
    f = Field{};
    describe(ctx, id, f);
    auto& original = scalarScratch.token;
    if (text != original) {
      size_t length = strlen(text);
      if (length >= sizeof(original)) return false;
      memcpy(original, text, length + 1);
    }
    auto& current = f.value;
    size_t len, used;
    if (string(original, original, sizeof(original), len) && len > f.capacity &&
        d->access(*static_cast<Context*>(ctx)->model, local, f, nullptr) &&
        string(f.value, current, sizeof(current), used))
      return used == f.capacity && !memcmp(original, current, used);
  }
  return false;
}
bool cover(void* ctx, unsigned id, const char* text, uint8_t* bitmap)
{
  unsigned local = id;
  auto d = descriptor(local);
  if (!d || strcmp(d->codec, "ModelIds") || !*text || *text == '#') return true;
  for (unsigned i = 0; i < 2; ++i) {
    char p[48];
    snprintf(p, sizeof(p), "header/modelId/%u/val", i);
    int child = resolve(ctx, p);
    if (child >= 0) {
      unsigned mask = 1u << (child % 8);
      if (bitmap[child / 8] & mask) return false;
      bitmap[child / 8] |= mask;
    }
  }
  return true;
}
bool emitSequence(void*, const char* path)
{
  return !strcmp(path, "mixData") || !strcmp(path, "expoData") ||
         !strcmp(path, "customSwitches");
}
Schema make(Context& c)
{
  prepare();
  return {count(),  &c,   field,        set,  known,    resolve, describe,
          sequence, seen, sizeof(seen), next, preserve, cover,   emitSequence};
}
}  // namespace
config_stream::Schema schema(ModelData& model)
{
  saveContext = {&model, false};
  return make(saveContext);
}
config_stream::Schema beginLoad()
{
  candidate = ModelData{};
#if defined(FUNCTION_SWITCHES)
  beginFunctions();
#endif
  memset(sensorStage, 0, sizeof(sensorStage));
  memset(candidateScriptSource, 0, sizeof(candidateScriptSource));
  candidate.rfAlarms.warning = 45;
  candidate.rfAlarms.critical = 42;
#if defined(FUNCTION_SWITCHES)
  for (unsigned x = 0; x < switchGetMaxSwitches(); ++x)
    if (switchIsCustomSwitch(x)) {
      unsigned i = switchGetCustomSwitchIdx(x);
      auto& sw = candidate.customSwitches[i];
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
  candidate.cfsSetGroupAlwaysOn(1, true);
#endif
#if !defined(COLORLCD)
  memset(monoStage, 0, sizeof(monoStage));
#else
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
#if !defined(COLORLCD)
  for (unsigned i = 0; i < MAX_TELEMETRY_SCREENS; ++i) {
    unsigned type = candidate.getTelemetryScreenType(i);
    candidate.screens[i] = monoStage[i][type == 1 ? 1 : type == 3 ? 2 : 0];
  }
#endif
  unsigned points = 0;
  for (auto& curve : candidate.curves) {
    int n = curve.points + 5;
    if (n < 2 || n > 17) {
      static char error[48];
      snprintf(error, sizeof(error), "Invalid curve %u point count: %d",
               unsigned(&curve - candidate.curves) + 1, n);
      return error;
    }
    points += curve.type ? 2 * n - 2 : n;
  }
  if (points > MAX_CURVE_POINTS) return "model curve capacity exceeded";
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
  memcpy(scriptSource, candidateScriptSource, sizeof(scriptSource));
}
namespace
{
const char* headerPaths[] = {"header/name",          "header/modelId",
                             "header/bitmap",        "header/labels",
                             "moduleData/0/type",    "moduleData/1/type",
                             "header/modelId/0/val", "header/modelId/1/val"};
bool headerField(void* ctx, unsigned id, Field& f)
{
  auto& h = *static_cast<Header*>(ctx);
  strcpy(f.path, headerPaths[id]);
  f.required = id != 1;
  f.available = true;
#if LEN_BITMAP_NAME == 0
  if (id == 2) f.available = false;
#endif
#if !defined(STORAGE_MODELSLIST)
  if (id == 3) f.available = false;
#endif
  if (!f.available || f.metadataOnly) return true;
  if (id == 0) return textValue(h.header.name, f, nullptr);
  if (id == 1) return modelIds(h.header.modelId, f, nullptr);
#if LEN_BITMAP_NAME > 0
  if (id == 2) return textValue(h.header.bitmap, f, nullptr);
#endif
#if defined(STORAGE_MODELSLIST)
  if (id == 3) return textValue(h.header.labels, f, nullptr);
#endif
  if (id >= 6)
    return formatInteger(f.value, sizeof(f.value), h.header.modelId[id - 6]);
  return id >= 4 && enumOutput(Module, h.moduleData[id - 4].type, f);
}
bool headerDescribe(void* ctx, unsigned id, Field& f)
{
  f.metadataOnly = true;
  return headerField(ctx, id, f);
}
bool headerLabelsField(void* ctx, unsigned id, Field& field)
{
  if (!headerField(ctx, id, field)) return false;
  field.available = field.available && id == 3;
  return true;
}
bool headerLabelsDescribe(void* ctx, unsigned id, Field& field)
{
  field.metadataOnly = true;
  return headerLabelsField(ctx, id, field);
}
bool headerSet(void* ctx, unsigned id, const char* text)
{
  auto& h = *static_cast<Header*>(ctx);
  Field f{};
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
int headerResolve(void*, const char* path)
{
  if (!strcmp(path, "header/modelId/0")) return 6;
  if (!strcmp(path, "header/modelId/1")) return 7;
  for (unsigned i = 0; i < 8; ++i)
    if (!strcmp(path, headerPaths[i])) return i;
  return -1;
}
bool headerKnown(void*, const char*) { return true; }
bool headerSequence(void*, const char* path)
{
  return !strcmp(path, "moduleData") || !strcmp(path, "header/modelId");
}
bool headerPreserve(void*, unsigned id, const char* text)
{
  return id == 1 && (!*text || *text == '#');
}
bool headerCover(void*, unsigned id, const char* text, uint8_t* bitmap)
{
  if (id != 1 || !*text || *text == '#') return true;
  if (bitmap[0] & 0xC0) return false;
  bitmap[0] |= 0xC0;
  return true;
}
}  // namespace
config_stream::Schema headerSchema(Header& h, bool labelsOnly)
{
  return {8,
          &h,
          labelsOnly ? headerLabelsField : headerField,
          headerSet,
          headerKnown,
          headerResolve,
          labelsOnly ? headerLabelsDescribe : headerDescribe,
          headerSequence,
          nullptr,
          0,
          nullptr,
          headerPreserve,
          headerCover};
}

}  // namespace model_config
