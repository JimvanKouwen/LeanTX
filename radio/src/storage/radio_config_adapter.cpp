// LeanTX radio configuration. GPL-2.0-or-later.
#include "edgetx.h"
#include "analogs.h"
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"
#include "radio_config_adapter.h"
#include <stdio.h>
#include <string.h>

namespace {
using namespace radio_config;
unsigned configVersion = 1;
struct EnumValue { const char* name; int value; };
const EnumValue backlightModes[] = {{"backlight_mode_off",0},{"backlight_mode_keys",1},{"backlight_mode_sticks",2},{"backlight_mode_all",3},{"backlight_mode_on",4},{nullptr,0}};
const EnumValue antennaModes[] = {{"MODE_INTERNAL",-2},{"MODE_ASK",-1},{"MODE_PER_MODEL",0},{"MODE_EXTERNAL",1},{nullptr,0}};
const EnumValue beepModes[] = {{"mode_quiet",-2},{"mode_alarms",-1},{"mode_nokeys",0},{"mode_all",1},{nullptr,0}};
const EnumValue hatsModes[] = {{"TRIMS_ONLY",0},{"KEYS_ONLY",1},{"SWITCHABLE",2},{"GLOBAL",3},{nullptr,0}};
const EnumValue moduleModes[] = {{"TYPE_NONE",0},{"TYPE_CROSSFIRE",MODULE_TYPE_CROSSFIRE},{nullptr,0}};
const EnumValue bluetoothModes[] = {{"OFF",0},{"TELEMETRY",1},{nullptr,0}};
enum Capability { Always, Mono, Color, Hats, Antenna, Rtc, KeyLight, Audio, InternalRf, Haptic, Bluetooth, Encoder, UartSampling, DeadZone, Imu, UsbCharge, InvertLcd, BacklightColor, Backlight };
bool available(Capability c) {
  switch(c) {
    case Always: return true;
#if defined(PCBX9E) || defined(PCBX9DP)
    case BacklightColor: return true;
#endif
#if !OLED_SCREEN
    case Backlight: return true;
#endif
#if !defined(COLORLCD)
    case Mono: return true;
#endif
#if defined(COLORLCD)
    case Color: return true;
#endif
#if defined(USE_HATS_AS_KEYS)
    case Hats: return true;
#endif
#if defined(EXTERNAL_ANTENNA)
    case Antenna: return true;
#endif
#if defined(RTCLOCK)
    case Rtc: return true;
#endif
#if defined(KEYS_BACKLIGHT_GPIO)
    case KeyLight: return true;
#endif
#if defined(AUDIO)
    case Audio: return true;
#endif
#if defined(INTERNAL_MODULE_CRSF)
    case InternalRf: return true;
#endif
#if defined(HAPTIC)
    case Haptic: return true;
#endif
#if defined(BLUETOOTH)
    case Bluetooth: return true;
#endif
#if defined(ROTARY_ENCODER_NAVIGATION)
    case Encoder: return true;
#endif
#if defined(STM32F4)
    case UartSampling: return true;
#endif
#if defined(STICK_DEAD_ZONE)
    case DeadZone: return true;
#endif
#if defined(IMU)
    case Imu: return true;
#endif
#if defined(COLORLCD) && defined(USB_CHARGE_CONTROL)
    case UsbCharge: return true;
#endif
#if !defined(COLORLCD) && LCD_W == 128
    case InvertLcd: return true;
#endif
    default: return false;
  }
}
#define RC_MONO RC_FIELD
#define RC_COLOR RC_FIELD
#define RC_HATS RC_FIELD
#define RC_F4 RC_FIELD
#define RC_DEADZONE RC_FIELD
#define RC_IMUF RC_FIELD
#define RC_CHARGE RC_FIELD
#define RC_INVERT RC_FIELD
#define RC_FIELD(key, member, lo, hi, offset, scale, cap, names) ID_##key,
enum ScalarId {
#include "radio_config_fields.inc"
  ScalarCount
};
#undef RC_FIELD
struct Scalar { const char* key; int64_t lo, hi; int offset, scale; Capability capability; const EnumValue* names; };
#define RC_FIELD(key, member, lo, hi, offset, scale, cap, names) {#key, lo, hi, offset, scale, cap, names},
const Scalar scalars[] = {
#include "radio_config_fields.inc"
};
#undef RC_FIELD
#undef RC_MONO
#undef RC_COLOR
#undef RC_HATS
#undef RC_F4
#undef RC_DEADZONE
#undef RC_IMUF
#undef RC_CHARGE
#undef RC_INVERT
#if !defined(COLORLCD)
#define RC_MONO RC_FIELD
#else
#define RC_MONO(...)
#endif
#if defined(COLORLCD)
#define RC_COLOR RC_FIELD
#else
#define RC_COLOR(...)
#endif
#if defined(USE_HATS_AS_KEYS)
#define RC_HATS RC_FIELD
#else
#define RC_HATS(...)
#endif
#if defined(STM32F4)
#define RC_F4 RC_FIELD
#else
#define RC_F4(...)
#endif
#if defined(STICK_DEAD_ZONE)
#define RC_DEADZONE RC_FIELD
#else
#define RC_DEADZONE(...)
#endif
#if defined(IMU)
#define RC_IMUF RC_FIELD
#else
#define RC_IMUF(...)
#endif
#if defined(COLORLCD) && defined(USB_CHARGE_CONTROL)
#define RC_CHARGE RC_FIELD
#else
#define RC_CHARGE(...)
#endif
#if !defined(COLORLCD) && LCD_W == 128
#define RC_INVERT RC_FIELD
#else
#define RC_INVERT(...)
#endif
int64_t scalarGet(unsigned id) {
  switch(id) {
#define RC_FIELD(key, member, lo, hi, offset, scale, cap, names) case ID_##key: return int64_t(g_eeGeneral.member) * scale + offset;
#include "radio_config_fields.inc"
#undef RC_FIELD
    default: return 0;
  }
}
void scalarSet(unsigned id, int64_t value) {
  switch(id) {
#define RC_FIELD(key, member, lo, hi, offset, scale, cap, names) case ID_##key: g_eeGeneral.member = (value - offset) / scale; break;
#include "radio_config_fields.inc"
#undef RC_FIELD
  }
}
#undef RC_MONO
#undef RC_COLOR
#undef RC_HATS
#undef RC_F4
#undef RC_DEADZONE
#undef RC_IMUF
#undef RC_CHARGE
#undef RC_INVERT
// Arrays have universal semantic shapes; HAL names identify physical controls.
// These bounds are schema limits, not target RAM array dimensions.
enum {
  Strings = ScalarCount, Source = Strings + 6, Calib = Source + 2,
  Sticks = Calib + 32 * 9, Pots = Sticks + 4 * 2,
  Switches = Pots + 16 * 3, Serial = Switches + 32 * 11,
  Flex = Serial + 3 * 2, Shortcuts = Flex + 16,
  Favorites = Shortcuts + 6, Version = Favorites + 12,
  Aliases = Version + 1, LegacySliders = Aliases + 5, FieldCount = LegacySliders + 16 * 2
};
static_assert(FieldCount <= MaxFields, "radio schema tracking capacity");
static_assert(MAX_CALIB_ANALOG_INPUTS <= 32 && MAX_STICKS <= 4 && MAX_POTS <= 16 &&
              MAX_SWITCHES <= 32 && MAX_FLEX_SWITCHES <= 16,
              "extend universal hardware binding capacity for this target");
const char* const stringKeys[] = {"ttsLanguage", "uiLanguage", "bluetoothName", "currModelFilename", "selectedTheme"};
const char* const calibKeys[] = {"mid", "spanNeg", "spanPos", "count", "steps/0", "steps/1", "steps/2", "steps/3", "steps/4"};
const char* const switchKeys[] = {"name", "type", "start", "onColorLuaOverride", "offColorLuaOverride", "onColor/r", "onColor/g", "onColor/b", "offColor/r", "offColor/g", "offColor/b"};
const char* const aliasKeys[] = {"telemetryBaudrate", "jitterFilter", "rotEncDirection", "auxSerialMode", "aux2SerialMode"};
const char* const ports[] = {"AUX1", "AUX2", "VCP"};
const char* const uartModes[] = {"NONE", "TELEMETRY_MIRROR", "reserved2", "reserved3", "reserved4", "LUA", "CLI", "GPS", "DEBUG", "SPACEMOUSE", "EXT_MODULE"};
const char* const potTypes[] = {"none", "without_detent", "with_detent", "slider", "multipos_switch", "axis_x", "axis_y", "switch"};
const char* const switchTypes[] = {"NONE", "TOGGLE", "2POS", "3POS", "GLOBAL"};
const char* const pages[] = {
 "NONE", "OPEN_QUICK_MENU", "MANAGE_MODELS", "MODEL_SETUP", "MODEL_INPUTS", "MODEL_MIXES", "MODEL_OUTPUTS", "MODEL_CURVES", "MODEL_SCRIPTS", "MODEL_TELEMETRY", "MODEL_NOTES", "RADIO_SETUP", "RADIO_HARDWARE", "RADIO_VERSION", "UI_THEMES", "UI_SETUP", "UI_SCREEN1", "UI_SCREEN2", "UI_SCREEN3", "UI_SCREEN4", "UI_SCREEN5", "UI_SCREEN6", "UI_SCREEN7", "UI_SCREEN8", "UI_SCREEN9", "UI_SCREEN10", "UI_ADD_PG", "TOOLS_APPS", "TOOLS_STORAGE", "TOOLS_RESET", "TOOLS_CHAN_MON", "TOOLS_STATS", "TOOLS_DEBUG", "APP"
};
bool number(Field& f, int64_t n) {
  if (!f.metadataOnly) snprintf(f.value, sizeof(f.value), "%lld", (long long)n);
  return true;
}
bool quoted(Field& f, const char* text, size_t max) {
  if (f.metadataOnly) return true;
  size_t n = 0; f.value[n++] = '"';
  for (size_t j = 0; j < max && text[j]; ++j) {
    unsigned char c = text[j];
    if (n + 3 >= sizeof(f.value) || c < 32) return false;
    if (c == '"' || c == '\\') f.value[n++] = '\\';
    f.value[n++] = c;
  }
  f.value[n++] = '"'; f.value[n] = 0; return true;
}
char* stringMember(unsigned i, size_t& size) {
  switch(i) {
    case 0: size = 2; return g_eeGeneral.ttsLanguage;
    case 1: size = 2; return g_eeGeneral.uiLanguage;
    case 2: size = sizeof(g_eeGeneral.bluetoothName); return g_eeGeneral.bluetoothName;
#if defined(COLORLCD)
    case 3: size = sizeof(g_eeGeneral.currModelFilename) - 1; return g_eeGeneral.currModelFilename;
    case 4: size = sizeof(g_eeGeneral.selectedTheme) - 1; return g_eeGeneral.selectedTheme;
#endif
  }
  return nullptr;
}
bool field(void*, unsigned id, Field& f) {
  f.available = true;
  f.required = true;
  if (id < ScalarCount) {
    auto& s = scalars[id]; strcpy(f.path, s.key);
    f.available = available(s.capability);
    auto n = scalarGet(id);
    if (s.lo == 0 && s.hi == 1) n = !!n;
    if (s.names) for (auto e = s.names; e->name; ++e) if (e->value == n) {
      strcpy(f.value, e->name); return true;
    }
    return number(f, n);
  }
  if (id < Source) {
    unsigned i = id - Strings;
    if (i == 5) return false;
    strcpy(f.path, stringKeys[i]);
    size_t size = 0; auto str = stringMember(i, size);
    f.available = str && (i != 2 || available(Bluetooth));
    return !f.available || quoted(f, str, size);
  }
  if (id < Calib) {
    strcpy(f.path, id == Source ? "backlightSrc" : "volumeSrc");
    if (f.metadataOnly) return true;
    int source = id == Source ? g_eeGeneral.backlightSrc : g_eeGeneral.volumeSrc;
    if (!source) { strcpy(f.value, "NONE"); return true; }
    bool invert = source < 0;
    if (invert) source = -source;
    const char* name = nullptr;
    if (source >= MIXSRC_FIRST_POT && source <= MIXSRC_LAST_POT)
      name = analogGetPhysicalName(ADC_INPUT_FLEX, source - MIXSRC_FIRST_POT);
    else if (source >= MIXSRC_FIRST_SWITCH && source <= MIXSRC_LAST_SWITCH)
      name = switchGetDefaultName(source - MIXSRC_FIRST_SWITCH);
#if defined(LUMINOSITY_SENSOR)
    else if (source == MIXSRC_LIGHT) name = "LIGHT";
#endif
    if (!name || !isSourceAvailableForBacklightOrVolume(source)) return false;
    char symbolic[40];
    if (strlen(name) + 2 > sizeof(symbolic)) return false;
    snprintf(symbolic, sizeof(symbolic), "%s%s", invert ? "!" : "", name);
    return quoted(f, symbolic, sizeof(symbolic));
  }
  if (id < Sticks) {
    unsigned i = (id - Calib) / 9, part = (id - Calib) % 9;
    if (i >= adcGetMaxCalibratedInputs() || i >= MAX_CALIB_ANALOG_INPUTS) return false;
    const char* name = adcGetInputName(i); if (!name) return false;
    snprintf(f.path, sizeof(f.path), "calib/%s/%s", name, calibKeys[part]);
    const auto& c = g_eeGeneral.calib[i];
    int pot = int(i) - adcGetInputOffset(ADC_INPUT_FLEX);
    bool multi = pot >= 0 && pot < adcGetMaxInputs(ADC_INPUT_FLEX) && getPotType(pot) == FLEX_MULTIPOS;
    f.available = (part >= 3) == multi;
    if (part < 3) return number(f, part == 0 ? c.mid : part == 1 ? c.spanNeg : c.spanPos);
#if XPOTS_MULTIPOS_COUNT > 0
    const auto& steps = reinterpret_cast<const StepsCalibData&>(c);
    return number(f, part == 3 ? (steps.count < XPOTS_MULTIPOS_COUNT ? steps.count : 0) : steps.steps[part - 4]);
#else
    f.available = false; return true;
#endif
  }
  if (id < Pots) {
    unsigned i = (id - Sticks) / 2, part = (id - Sticks) % 2;
    if (i >= adcGetMaxInputs(ADC_INPUT_MAIN)) return false;
    snprintf(f.path, sizeof(f.path), "sticksConfig/%u/%s", i, part ? "inv" : "name");
    return part ? number(f, getStickInversion(i)) : quoted(f, analogGetCustomLabel(ADC_INPUT_MAIN, i), LEN_ANA_NAME);
  }
  if (id < Switches) {
    unsigned i = (id - Pots) / 3, part = (id - Pots) % 3;
    if (i >= adcGetMaxInputs(ADC_INPUT_FLEX)) return false;
    const char* name = analogGetPhysicalName(ADC_INPUT_FLEX, i); if (!name) return false;
    snprintf(f.path, sizeof(f.path), "potsConfig/%s/%s", name, part == 0 ? "type" : part == 1 ? "inv" : "name");
    if (part == 0) { strcpy(f.value, potTypes[getPotType(i) & 7]); return true; }
    return part == 1 ? number(f, getPotInversion(i)) : quoted(f, analogGetCustomLabel(ADC_INPUT_FLEX, i), LEN_ANA_NAME);
  }
  if (id < Serial) {
    unsigned i = (id - Switches) / 11, part = (id - Switches) % 11;
    if (i >= switchGetMaxSwitches() || i >= MAX_SWITCHES) return false;
    const char* name = switchGetDefaultName(i); if (!name) return false;
    snprintf(f.path, sizeof(f.path), "switchConfig/%s/%s", name, switchKeys[part]);
    auto& s = g_eeGeneral.switchConfig[i];
    if (part == 0) return quoted(f, s.name, LEN_SWITCH_NAME);
    if (part == 1) { strcpy(f.value, switchTypes[s.type <= 4 ? s.type : 0]); return true; }
    f.available = false;
#if defined(FUNCTION_SWITCHES)
    f.available = switchIsCustomSwitch(i);
    if (part == 2) { strcpy(f.value, s.start == 0 ? "OFF" : s.start == 1 ? "ON" : "PREVIOUS"); return true; }
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
    if (part == 3) return number(f, s.onColorLuaOverride);
    if (part == 4) return number(f, s.offColorLuaOverride);
    const auto& c = part < 8 ? s.onColor : s.offColor;
    return number(f, part == 5 || part == 8 ? c.r : part == 6 || part == 9 ? c.g : c.b);
#else
    f.available = false;
#endif
#endif
    return true;
  }
  if (id < Flex) {
    unsigned i = (id - Serial) / 2, part = (id - Serial) % 2;
    snprintf(f.path, sizeof(f.path), "serialPort/%s/%s", ports[i], part ? "power" : "mode");
    const auto* port = serialGetPort(i);
    f.available = port && (!part || port->set_pwr);
    if (part) return number(f, serialGetPower(i));
    int mode = serialGetMode(i);
    strcpy(f.value, mode >= 0 && mode < UART_MODE_COUNT ? uartModes[mode] : "NONE"); return true;
  }
  if (id < Shortcuts) {
    unsigned i = id - Flex;
    if (int(i) >= int(MAX_FLEX_SWITCHES)) return false;
    const char* name = switchGetDefaultName(i + switchGetMaxSwitches()); if (!name) return false;
    snprintf(f.path, sizeof(f.path), "flexSwitches/%s/channel", name);
    int channel = switchGetFlexConfig_raw(i);
    const char* input = channel >= 0 ? analogGetPhysicalName(ADC_INPUT_FLEX, channel) : nullptr;
    return quoted(f, input ? input : "NONE", 32);
  }
  if (id < Version) {
    bool fav = id >= Favorites; unsigned i = id - (fav ? Favorites : Shortcuts);
    snprintf(f.path, sizeof(f.path), "%s/%u/shortcut", fav ? "qmFavorites" : "keyShortcuts", i);
    f.available = available(Color);
    if (f.metadataOnly) return true;
#if defined(COLORLCD)
    unsigned page = fav ? g_eeGeneral.qmFavorites[i].shortcut : g_eeGeneral.keyShortcuts[i].shortcut;
    if (page > QM_APP) return false;
    if (page == QM_APP) {
      char name[RadioData::ToolNameCapacity + 4];
      snprintf(name, sizeof(name), "APP,%s", g_eeGeneral.configToolName(fav, i));
      return quoted(f, name, sizeof(name));
    } else strcpy(f.value, pages[page]);
#endif
    return true;
  }
  if (id == Version) { strcpy(f.path, "configVersion"); return number(f, configVersion); }
  f.required = false;
  if (id < LegacySliders) {
    unsigned i = id - Aliases; strcpy(f.path, aliasKeys[i]);
    if (i == 0) { f.available = available(InternalRf); return number(f, g_eeGeneral.internalModuleBaudrate); }
    if (i == 1) return number(f, g_eeGeneral.noJitterFilter);
    if (i == 2) { f.available = available(Encoder); return number(f, g_eeGeneral.rotEncMode); }
    f.available = serialGetPort(i - 3);
    int mode = serialGetMode(i - 3);
    if (mode < 0 || mode >= UART_MODE_COUNT) return false;
    snprintf(f.value, sizeof(f.value), "MODE_%s", uartModes[mode]); return true;
  }
  unsigned i = (id - LegacySliders) / 2, part = (id - LegacySliders) % 2;
  if (i >= adcGetMaxInputs(ADC_INPUT_FLEX)) return false;
  const char* name = analogGetPhysicalName(ADC_INPUT_FLEX, i);
  if (!name) return false;
  snprintf(f.path, sizeof(f.path), "slidersConfig/%s/%s", name, part ? "name" : "type");
  if (part) return quoted(f, analogGetCustomLabel(ADC_INPUT_FLEX, i), LEN_ANA_NAME);
  return number(f, getPotType(i));
}
int choice(const char* text, const char* const* choices, unsigned count) {
  for (unsigned i = 0; i < count; ++i) if (!strcasecmp(text, choices[i])) return i;
  int64_t n; return integer(text, 0, count - 1, n) ? n : -1;
}
bool isText(const char* text, const char* decoded) {
  // YAML collections, nulls, booleans and numbers are not string settings.
  if (*text != '\'' && *text != '"') {
    if (!*text || strchr("[{|>", *text) || !strcmp(text, "null") ||
        !strcmp(text, "~") || !strcmp(text, "true") || !strcmp(text, "false") ||
        isdigit((unsigned char)*text) ||
        ((*text == '-' || *text == '+' || *text == '.') && isdigit((unsigned char)text[1]))) return false;
  }
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(decoded); *p; ++p)
    if (*p < 32 || *p == 127) return false;
  return true;
}
bool set(void* context, unsigned id, const char* text) {
  const auto phase = static_cast<RadioConfigLoadPhase>(reinterpret_cast<uintptr_t>(context));
  const bool hardwareField = (id >= Pots && id < Switches && (id - Pots) % 3 == 0) ||
      (id >= LegacySliders && (id - LegacySliders) % 2 == 0);
  if ((phase == RadioConfigLoadPhase::Hardware && !hardwareField) ||
      (phase == RadioConfigLoadPhase::Values && hardwareField)) return true;
  int64_t n;
  if (id < ScalarCount) {
    const auto& s = scalars[id]; bool valid = false;
    if (s.names) for (auto e = s.names; e->name; ++e) if (!strcmp(text, e->name)) { n = e->value; valid = true; break; }
    if (!valid && !integer(text, s.lo, s.hi, n)) return false;
    if (n < s.lo || n > s.hi || (n - s.offset) % s.scale) return false;
    if (id == ID_internalModule && !isInternalModuleSupported(n)) return false;
    scalarSet(id, n); return true;
  }
  char decoded[MaxValue] = {}; size_t len;
  if (!string(text, decoded, sizeof(decoded) - 1, len)) return false;
  if (id < Source) {
    size_t size = 0; char* dst = stringMember(id - Strings, size);
    if (!dst || len > size || !isText(text, decoded)) return false;
    if (id - Strings < 2 && (len != 2 || !isalpha((unsigned char)decoded[0]) || !isalpha((unsigned char)decoded[1]))) return false;
    memset(dst, 0, size); memcpy(dst, decoded, len); return true;
  }
  if (id < Calib) {
    int source = 0;
    const bool inverted = *decoded == '!';
    const char* name = decoded + (inverted ? 1 : 0);
    if (strcmp(name, "NONE") && strcmp(name, "0")) {
      int index = analogLookupPhysicalIdx(ADC_INPUT_FLEX, name, strlen(name));
      if (index >= 0) source = MIXSRC_FIRST_POT + index;
      else {
        index = switchLookupIdx(name, strlen(name));
        if (index >= 0) source = MIXSRC_FIRST_SWITCH + index;
#if defined(LUMINOSITY_SENSOR)
        else if (!strcmp(name, "LIGHT")) source = MIXSRC_LIGHT;
#endif
        else return false;
      }
      if (!isSourceAvailableForBacklightOrVolume(source)) return false;
    }
    if (inverted) source = -source;
    if (id == Source) g_eeGeneral.backlightSrc = source; else g_eeGeneral.volumeSrc = source;
    return true;
  }
  if (id < Sticks) {
    unsigned i = (id - Calib) / 9, part = (id - Calib) % 9;
    auto& c = g_eeGeneral.calib[i];
    if (part >= 3) {
#if XPOTS_MULTIPOS_COUNT > 0
      if (!integer(text, 0, part == 3 ? XPOTS_MULTIPOS_COUNT - 1 : 255, n)) return false;
      auto& steps = reinterpret_cast<StepsCalibData&>(c);
      if (part == 3) steps.count = n; else steps.steps[part - 4] = n;
      return true;
#else
      return false;
#endif
    }
    if (!integer(text, part ? 1 : -32768, 32767, n)) return false;
    if (!part) c.mid = n; else if (part == 1) c.spanNeg = n; else c.spanPos = n;
    return true;
  }
  if (id < Pots) {
    unsigned i = (id - Sticks) / 2, part = (id - Sticks) % 2;
    if (!part) { if (len > LEN_ANA_NAME || !isText(text, decoded)) return false; analogSetCustomLabel(ADC_INPUT_MAIN, i, decoded, len); }
    else { if (!integer(text, 0, 1, n)) return false; setStickInversion(i, n); }
    return true;
  }
  if (id < Switches) {
    unsigned i = (id - Pots) / 3, part = (id - Pots) % 3;
    if (part == 0) { int type = choice(decoded, potTypes, 8); if (type < 0 || !isPotTypeAvailable(type)) return false; if (type == FLEX_MULTIPOS && getPotType(i) != FLEX_MULTIPOS) {
      auto& c = g_eeGeneral.calib[adcGetInputOffset(ADC_INPUT_FLEX) + i];
      memset(&c, 0, sizeof(c));
    }
    setPotType(i, type); }
    else if (part == 1) { if (!integer(text, 0, 1, n)) return false; setPotInversion(i, n); }
    else { if (len > LEN_ANA_NAME || !isText(text, decoded)) return false; analogSetCustomLabel(ADC_INPUT_FLEX, i, decoded, len); }
    return true;
  }
  if (id < Serial) {
    unsigned i = (id - Switches) / 11, part = (id - Switches) % 11;
    auto& s = g_eeGeneral.switchConfig[i];
    if (!part) { if (len > LEN_SWITCH_NAME || !isText(text, decoded)) return false; memset(s.name, 0, LEN_SWITCH_NAME); memcpy(s.name, decoded, len); return true; }
    if (part == 1) { int type = choice(decoded, switchTypes, switchIsCustomSwitch(i) ? 5 : 4); if (type < 0) return false; s.type = type; return true; }
#if defined(FUNCTION_SWITCHES)
    if (part == 2) { const char* names[] = {"OFF", "ON", "PREVIOUS"}; int start = choice(decoded, names, 3); if (start < 0) return false; s.start = start; return true; }
    if (!integer(text, 0, part < 5 ? 1 : 255, n)) return false;
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
    if (part == 3) s.onColorLuaOverride = n;
    else if (part == 4) s.offColorLuaOverride = n;
    else {
      auto& c = part < 8 ? s.onColor : s.offColor;
      if (part == 5 || part == 8) c.r = n;
      else if (part == 6 || part == 9) c.g = n;
      else c.b = n;
    }
    return true;
#endif
#endif
    return false;
  }
  if (id < Flex) {
    unsigned i = (id - Serial) / 2, part = (id - Serial) % 2;
    if (part) { if (!integer(text, 0, 1, n)) return false; serialSetPower(i, n); }
    else { int mode = choice(decoded, uartModes, UART_MODE_COUNT); if (mode < 0 || (mode != UART_MODE_NONE && !isSerialModeAvailable(i, mode))) return false; serialSetMode(i, mode); }
    return true;
  }
  if (id < Shortcuts) {
    int channel = !strcmp(decoded, "NONE") ? -1 : analogLookupPhysicalIdx(ADC_INPUT_FLEX, decoded, len);
    if (channel < 0 && strcmp(decoded, "NONE")) return false;
    switchConfigFlex_raw(id - Flex, channel); return true;
  }
  if (id < Version) {
#if defined(COLORLCD)
    bool fav = id >= Favorites; unsigned i = id - (fav ? Favorites : Shortcuts);
    bool app = !strncmp(decoded, "APP,", 4);
    int page = app ? int(QM_APP) : choice(decoded, pages, sizeof(pages) / sizeof(*pages));
    if (page < 0) return false;
    if (!g_eeGeneral.configSetToolName(fav, i, app ? decoded + 4 : "")) return false;
    if (fav) g_eeGeneral.qmFavorites[i].shortcut = page;
    else g_eeGeneral.keyShortcuts[i].shortcut = page;
    return true;
#else
    return false;
#endif
  }
  if (id == Version) {
    if (!integer(text, 1, 65535, n)) return false;
    configVersion = n; return true;
  }
  if (id < LegacySliders) {
    unsigned i = id - Aliases;
    if (i == 0) return set(nullptr, ID_internalModuleBaudrate, text);
    if (i == 1) return set(nullptr, ID_noJitterFilter, text);
    if (i == 2) return set(nullptr, ID_rotEncMode, text);
    const char* modeName = !strncmp(decoded, "MODE_", 5) ? decoded + 5 : decoded;
    int mode = choice(modeName, uartModes, UART_MODE_COUNT);
    if (mode < 0 || (mode != UART_MODE_NONE && !isSerialModeAvailable(i - 3, mode))) return false;
    serialSetMode(i - 3, mode); return true;
  }
  unsigned i = (id - LegacySliders) / 2, part = (id - LegacySliders) % 2;
  if (part) {
    if (len > LEN_ANA_NAME || !isText(text, decoded)) return false;
    analogSetCustomLabel(ADC_INPUT_FLEX, i, decoded, len);
  } else {
    if (!strcmp(decoded, "none")) setPotType(i, FLEX_NONE);
    else if (!strcmp(decoded, "with_detent") || !strcmp(decoded, "slider")) setPotType(i, FLEX_SLIDER);
    else {
      if (!integer(text, 0, 7, n) || !isPotTypeAvailable(n)) return false;
      setPotType(i, n);
    }
  }
  return true;
}
bool describe(void* context, unsigned id, Field& f) {
  f.metadataOnly = true;
  return field(context, id, f);
}
int resolve(void*, const char* path) {
  if (!strchr(path, '/')) {
    for (unsigned i = 0; i < ScalarCount; ++i) if (!strcmp(path, scalars[i].key)) return i;
    for (unsigned i = 0; i < 5; ++i) if (!strcmp(path, stringKeys[i])) return Strings + i;
    for (unsigned i = 0; i < 5; ++i) if (!strcmp(path, aliasKeys[i])) return Aliases + i;
    if (!strcmp(path, "backlightSrc")) return Source;
    if (!strcmp(path, "volumeSrc")) return Source + 1;
    return !strcmp(path, "configVersion") ? Version : -1;
  }
  const char* first = strchr(path, '/');
  const char* second = strchr(first + 1, '/');
  if (!second) return -1;
  char item[32]; size_t length = second - first - 1;
  if (!length || length >= sizeof(item)) return -1;
  memcpy(item, first + 1, length); item[length] = 0;
  const char* leaf = second + 1;
  int64_t numericIndex;
  if (!strncmp(path, "sticksConfig/", 13) && integer(item, 0, 3, numericIndex))
    return !strcmp(leaf, "name") ? Sticks + numericIndex * 2 : !strcmp(leaf, "inv") ? Sticks + numericIndex * 2 + 1 : -1;
  if (!strncmp(path, "potsConfig/", 11)) {
    int i = analogLookupPhysicalIdx(ADC_INPUT_FLEX, item, length);
    if (i < 0 || i >= 16) return -1;
    return !strcmp(leaf, "type") ? Pots + i * 3 : !strcmp(leaf, "inv") ? Pots + i * 3 + 1 : !strcmp(leaf, "name") ? Pots + i * 3 + 2 : -1;
  }
  if (!strncmp(path, "switchConfig/", 13)) {
    int i = switchGetIndexFromName(item);
    if (i < 0 || i >= 32) return -1;
    for (unsigned part = 0; part < 11; ++part) if (!strcmp(leaf, switchKeys[part])) return Switches + i * 11 + part;
    return -1;
  }
  if (!strncmp(path, "serialPort/", 11)) {
    for (unsigned i = 0; i < 3; ++i) if (!strcmp(item, ports[i]))
      return !strcmp(leaf, "mode") ? Serial + i * 2 : !strcmp(leaf, "power") ? Serial + i * 2 + 1 : -1;
    return -1;
  }
  if (!strncmp(path, "flexSwitches/", 13) && !strcmp(leaf, "channel")) {
    for (unsigned i = 0; i < unsigned(MAX_FLEX_SWITCHES); ++i) {
      const char* name = switchGetDefaultName(switchGetMaxSwitches() + i);
      if (name && !strcmp(item, name)) return Flex + i;
    }
    return -1;
  }
  if (!strcmp(leaf, "shortcut")) {
    if (!strncmp(path, "keyShortcuts/", 13) && integer(item, 0, 5, numericIndex)) return Shortcuts + numericIndex;
    if (!strncmp(path, "qmFavorites/", 12) && integer(item, 0, 11, numericIndex)) return Favorites + numericIndex;
  }
  if (!strncmp(path, "slidersConfig/", 14)) {
    const char* end = strchr(path + 14, '/'); if (!end) return -1;
    int index = analogLookupPhysicalIdx(ADC_INPUT_FLEX, path + 14, end - path - 14);
    if (index < 0 && end - path == 16) {
      const char* name = !strncmp(path + 14, "LS", 2) ? "SL1" : !strncmp(path + 14, "RS", 2) ? "SL2" : "";
      index = analogLookupPhysicalIdx(ADC_INPUT_FLEX, name, strlen(name));
    }
    if (index < 0 || index >= 16) return -1;
    if (!strcmp(end + 1, "type")) return LegacySliders + index * 2;
    if (!strcmp(end + 1, "name")) return LegacySliders + index * 2 + 1;
    return -1;
  }
  if (strncmp(path, "calib/", 6)) return -1;
  const char* end = strchr(path + 6, '/'); if (!end) return -1;
  char name[32]; size_t len = end - (path + 6);
  if (len >= sizeof(name)) return -1;
  memcpy(name, path + 6, len); name[len] = 0;
  int index = adcGetInputIdx(name, len);
  if (index < 0) {
    const char* oldNames[] = {"Rud", "Ele", "Thr", "Ail"};
    for (unsigned i = 0; i < 4; ++i) if (!strcmp(name, oldNames[i])) index = i;
  }
  if (index < 0) { int64_t n; if (integer(name, 0, 31, n)) index = n; }
  if (index < 0 || index >= adcGetMaxCalibratedInputs()) return -1;
  for (unsigned p = 0; p < 9; ++p) if (!strcmp(end + 1, calibKeys[p])) return Calib + index * 9 + p;
  return -1;
}
bool known(void*, const char* path) {
  for (const auto& s : scalars) if (!strcmp(path, s.key)) return true;
  for (const char* key : aliasKeys) if (!strcmp(path, key)) return true;
  for (const char* key : stringKeys) if (!strcmp(path, key)) return true;
  struct Shape { const char* root; const char* leaves; };
  const Shape shapes[] = {
    {"calib", "mid|spanNeg|spanPos|count|steps|steps/0|steps/1|steps/2|steps/3|steps/4"},
    {"sticksConfig", "name|inv"}, {"slidersConfig", "name|type"},
    {"potsConfig", "name|type|inv"},
    {"switchConfig", "name|type|start|onColorLuaOverride|offColorLuaOverride|onColor|offColor|onColor/r|onColor/g|onColor/b|offColor/r|offColor/g|offColor/b"},
    {"serialPort", "mode|power"}, {"flexSwitches", "channel"},
    {"keyShortcuts", "shortcut"}, {"qmFavorites", "shortcut"}
  };
  for (const auto& shape : shapes) {
    size_t n = strlen(shape.root);
    if (strncmp(path, shape.root, n)) continue;
    if (!path[n]) return true;
    if (path[n] != '/') continue;
    const char* leaf = strchr(path + n + 1, '/');
    if (!leaf) return true; // Hardware instance mapping, even on another target.
    ++leaf;
    size_t len = strlen(leaf);
    for (const char* p = shape.leaves; *p;) {
      const char* end = strchr(p, '|');
      size_t count = end ? size_t(end - p) : strlen(p);
      if (count == len && !strncmp(p, leaf, len)) return true;
      if (!end) break;
      p = end + 1;
    }
    return false;
  }
  return !strcmp(path, "configVersion") || !strcmp(path, "backlightSrc") || !strcmp(path, "volumeSrc");
}
}
radio_config::Schema radioSettingsSchema(RadioConfigLoadPhase phase) { return {FieldCount, reinterpret_cast<void*>(uintptr_t(phase)), field, set, known, resolve, describe}; }

void resetRadioConfigMetadata() { configVersion = 1; }
