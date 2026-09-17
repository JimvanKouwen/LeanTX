// LeanTX radio configuration. GPL-2.0-or-later.
#include "edgetx.h"
#include "analogs.h"
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"
#include "radio_config_adapter.h"
#include <stdio.h>
#include <string.h>

namespace {
using namespace config_stream;
unsigned configVersion = 1;
// Isolated transaction state. Never point the global radio at this object:
// readers and hardware callbacks continue to see the live settings until commit.
struct Candidate {
  RadioData radio;
  unsigned version;
  char labels[MAX_CALIB_ANALOG_INPUTS][LEN_ANA_NAME + 1];
  int8_t flex[MAX_FLEX_SWITCHES ? MAX_FLEX_SWITCHES : 1];
  uint8_t powerSeen;
  // Three canonical ports plus two legacy mode aliases, in document order.
  // Replay after EOF to retain the first-valid-assignment conflict semantics.
  struct SerialMode { uint8_t port, mode; } modes[MAX_SERIAL_PORTS + 2];
  uint8_t modeCount;
  struct Calibration {
    int16_t normal[3];
    uint8_t steps[6];
    uint16_t seen, invalid;
    bool stepsNotMapping;
  } calibration[MAX_CALIB_ANALOG_INPUTS];
#if defined(COLORLCD)
  char tools[MAX_KEY_SHORTCUTS + MAX_QM_FAVORITES][RadioData::ToolNameCapacity];
#endif
};
static Candidate candidate;
static Field defaultField;
static_assert(sizeof(Candidate) <= 4096, "radio candidate fixed RAM budget");
RadioData& settings(void* context) {
  return context ? static_cast<Candidate*>(context)->radio : g_eeGeneral;
}
unsigned potType(const RadioData& radio, unsigned i) {
  return (radio.potsConfig >> (POT_CFG_BITS * i)) & ((1u << POT_CFG_TYPE_BITS) - 1);
}
void potTypeSet(RadioData& radio, unsigned i, unsigned type) {
  const potconfig_t mask = ((potconfig_t(1) << POT_CFG_TYPE_BITS) - 1) << (POT_CFG_BITS * i);
  radio.potsConfig = (radio.potsConfig & ~mask) | (potconfig_t(type) << (POT_CFG_BITS * i));
}
void labelSet(void* context, unsigned type, unsigned i, const char* text, size_t len) {
  if (!context) { analogSetCustomLabel(type, i, text, len); return; }
  auto& label = static_cast<Candidate*>(context)->labels[adcGetInputOffset(type) + i];
  memset(label, 0, sizeof(label));
  memcpy(label, text, len);
}
void modeSet(RadioData& radio, unsigned port, unsigned mode) {
  const unsigned shift = port * SERIAL_CONF_BITS_PER_PORT;
  radio.serialPort = (radio.serialPort & ~(SERIAL_CONF_MODE_MASK << shift)) | (mode << shift);
}
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
int64_t scalarGet(unsigned id, const RadioData& radio) {
  switch(id) {
#define RC_FIELD(key, member, lo, hi, offset, scale, cap, names) case ID_##key: return int64_t(radio.member) * scale + offset;
#include "radio_config_fields.inc"
#undef RC_FIELD
    default: return 0;
  }
}
void scalarSet(RadioData& radio, unsigned id, int64_t value) {
  switch(id) {
#define RC_FIELD(key, member, lo, hi, offset, scale, cap, names) case ID_##key: radio.member = (value - offset) / scale; break;
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
 "NONE", "OPEN_QUICK_MENU", "MANAGE_MODELS", "MODEL_SETUP", "MODEL_CHANNEL_MAPPING", "MODEL_TELEMETRY", "MODEL_NOTES", "RADIO_SETUP", "RADIO_HARDWARE", "RADIO_VERSION", "UI_THEMES", "UI_SETUP", "UI_SCREEN1", "UI_SCREEN2", "UI_SCREEN3", "UI_SCREEN4", "UI_SCREEN5", "UI_SCREEN6", "UI_SCREEN7", "UI_SCREEN8", "UI_SCREEN9", "UI_SCREEN10", "UI_ADD_PG", "TOOLS_APPS", "TOOLS_STORAGE", "TOOLS_RESET", "TOOLS_CHAN_MON", "TOOLS_STATS", "TOOLS_DEBUG", "APP"
};
#if defined(COLORLCD)
static_assert(sizeof(pages) / sizeof(*pages) == QM_APP + 1, "quick menu page names must match QMPage");
#endif
bool number(Field& f, int64_t n) {
  return f.metadataOnly || formatInteger(f.value, sizeof(f.value), n);
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
char* stringMember(unsigned i, size_t& size, RadioData& radio = g_eeGeneral) {
  switch(i) {
    case 0: size = 2; return radio.ttsLanguage;
    case 1: size = 2; return radio.uiLanguage;
    case 2: size = sizeof(radio.bluetoothName); return radio.bluetoothName;
#if defined(COLORLCD)
    case 3: size = sizeof(radio.currModelFilename) - 1; return radio.currModelFilename;
    case 4: size = sizeof(radio.selectedTheme) - 1; return radio.selectedTheme;
#endif
  }
  return nullptr;
}
bool field(void* context, unsigned id, Field& f) {
  auto& radio = settings(context);
  auto* pending = static_cast<Candidate*>(context);
  // Transaction-only marker for the steps container: its shape is relevant
  // only if the final pot type selects multiposition calibration.
  if (context && id >= FieldCount) {
    const unsigned i = id - FieldCount;
    if (i >= adcGetMaxCalibratedInputs()) return false;
    const char* name = adcGetInputName(i);
    if (!name) return false;
    f.available = true;

    return true;
  }
  f.available = true;

  if (id < ScalarCount) {
    auto& s = scalars[id];
    f.available = available(s.capability);
    auto n = scalarGet(id, radio);
    if (s.lo == 0 && s.hi == 1) n = !!n;
    if (s.names) for (auto e = s.names; e->name; ++e) if (e->value == n) {
      strcpy(f.value, e->name); return true;
    }
    return number(f, n);
  }
  if (id < Source) {
    unsigned i = id - Strings;
    if (i == 5) return false;
    size_t size = 0; auto str = stringMember(i, size, radio);
    f.available = str && (i != 2 || available(Bluetooth));
    return !f.available || quoted(f, str, size);
  }
  if (id < Calib) {
    if (f.metadataOnly) return true;
    int source = id == Source ? radio.backlightSrc : radio.volumeSrc;
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
    const auto& c = radio.calib[i];
    int pot = int(i) - adcGetInputOffset(ADC_INPUT_FLEX);
    bool multi = pot >= 0 && pot < adcGetMaxInputs(ADC_INPUT_FLEX) && potType(radio, pot) == FLEX_MULTIPOS;
    f.available = (part >= 3) == multi;
    if (part < 3) return number(f, part == 0 ? c.mid : part == 1 ? c.spanNeg : c.spanPos);
#if XPOTS_MULTIPOS_COUNT > 0
    const auto& steps = c.multipos;
    return number(f, part == 3 ? (steps.count < XPOTS_MULTIPOS_COUNT ? steps.count : 0) : steps.steps[part - 4]);
#else
    f.available = false; return true;
#endif
  }
  if (id < Pots) {
    unsigned i = (id - Sticks) / 2, part = (id - Sticks) % 2;
    if (i >= adcGetMaxInputs(ADC_INPUT_MAIN)) return false;
    return part ? number(f, ((radio.stickInvert >> i) & 1)) : quoted(f, (pending ? pending->labels[adcGetInputOffset(ADC_INPUT_MAIN) + i] : analogGetCustomLabel(ADC_INPUT_MAIN, i)), LEN_ANA_NAME);
  }
  if (id < Switches) {
    unsigned i = (id - Pots) / 3, part = (id - Pots) % 3;
    if (i >= adcGetMaxInputs(ADC_INPUT_FLEX)) return false;
    const char* name = analogGetPhysicalName(ADC_INPUT_FLEX, i); if (!name) return false;
    if (part == 0) { strcpy(f.value, potTypes[potType(radio, i) & 7]); return true; }
    return part == 1 ? number(f, ((radio.potsConfig >> (POT_CFG_BITS * i + POT_CFG_TYPE_BITS)) & 1)) : quoted(f, (pending ? pending->labels[adcGetInputOffset(ADC_INPUT_FLEX) + i] : analogGetCustomLabel(ADC_INPUT_FLEX, i)), LEN_ANA_NAME);
  }
  if (id < Serial) {
    unsigned i = (id - Switches) / 11, part = (id - Switches) % 11;
    if (i >= switchGetMaxSwitches() || i >= MAX_SWITCHES) return false;
    const char* name = switchGetDefaultName(i); if (!name) return false;
    auto& s = radio.switchConfig[i];
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
    const auto* port = serialGetPort(i);
    f.available = port && (!part || port->set_pwr);
    if (part) return number(f, ((radio.serialPort >> (i * SERIAL_CONF_BITS_PER_PORT + SERIAL_CONF_POWER_BIT)) & 1));
    int mode = ((radio.serialPort >> (i * SERIAL_CONF_BITS_PER_PORT)) & SERIAL_CONF_MODE_MASK);
    strcpy(f.value, mode >= 0 && mode < UART_MODE_COUNT ? uartModes[mode] : "NONE"); return true;
  }
  if (id < Shortcuts) {
    unsigned i = id - Flex;
    if (int(i) >= int(MAX_FLEX_SWITCHES)) return false;
    const char* name = switchGetDefaultName(i + switchGetMaxSwitches()); if (!name) return false;
    int channel = (pending ? pending->flex[i] : switchGetFlexConfig_raw(i));
    const char* input = channel >= 0 ? analogGetPhysicalName(ADC_INPUT_FLEX, channel) : nullptr;
    return quoted(f, input ? input : "NONE", 32);
  }
  if (id < Version) {
    f.available = available(Color);
    if (f.metadataOnly) return true;
#if defined(COLORLCD)
    bool fav = id >= Favorites; unsigned i = id - (fav ? Favorites : Shortcuts);
    unsigned page = fav ? radio.qmFavorites[i].shortcut : radio.keyShortcuts[i].shortcut;
    if (page > QM_APP) return false;
    if (page == QM_APP) {
      char name[RadioData::ToolNameCapacity + 4];
      snprintf(name, sizeof(name), "APP,%s", (pending ? pending->tools[(fav ? MAX_KEY_SHORTCUTS : 0) + i] : radio.configToolName(fav, i)));
      return quoted(f, name, sizeof(name));
    } else strcpy(f.value, pages[page]);
#endif
    return true;
  }
  if (id == Version) { return number(f, pending ? pending->version : configVersion); }

  if (id < LegacySliders) {
    unsigned i = id - Aliases;
    if (i == 0) { f.available = available(InternalRf); return number(f, radio.internalModuleBaudrate); }
    if (i == 1) return number(f, radio.noJitterFilter);
    if (i == 2) { f.available = available(Encoder); return number(f, radio.rotEncMode); }
    f.available = serialGetPort(i - 3);
    int mode = serialGetMode(i - 3);
    if (mode < 0 || mode >= UART_MODE_COUNT) return false;
    snprintf(f.value, sizeof(f.value), "MODE_%s", uartModes[mode]); return true;
  }
  unsigned i = (id - LegacySliders) / 2, part = (id - LegacySliders) % 2;
  if (i >= adcGetMaxInputs(ADC_INPUT_FLEX)) return false;
  const char* name = analogGetPhysicalName(ADC_INPUT_FLEX, i);
  if (!name) return false;
  if (part) return quoted(f, (pending ? pending->labels[adcGetInputOffset(ADC_INPUT_FLEX) + i] : analogGetCustomLabel(ADC_INPUT_FLEX, i)), LEN_ANA_NAME);
  return number(f, potType(radio, i));
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
bool serialModeSet(void* context, unsigned port, int mode) {
  if (mode < 0) return false;
  if (context) {
    auto& pending = *static_cast<Candidate*>(context);
    if (pending.modeCount == DIM(pending.modes)) return false;
    pending.modes[pending.modeCount++] = {uint8_t(port), uint8_t(mode)};
    return true;
  }
  if (mode != UART_MODE_NONE && !isSerialModeAvailable(port, mode)) return false;
  modeSet(g_eeGeneral, port, mode);
  return true;
}
bool set(void* context, unsigned id, const char* text) {
  auto& radio = settings(context);
  auto* pending = static_cast<Candidate*>(context);
  if (pending && id >= FieldCount) {
    pending->calibration[id - FieldCount].stepsNotMapping = *text != 0;
    return true;
  }
  int64_t n;
  if (id < ScalarCount) {
    const auto& s = scalars[id]; bool valid = false;
    if (s.names) for (auto e = s.names; e->name; ++e) if (!strcmp(text, e->name)) { n = e->value; valid = true; break; }
    if (!valid && !integer(text, s.lo, s.hi, n)) return false;
    if (n < s.lo || n > s.hi || (n - s.offset) % s.scale) return false;
    if (id == ID_internalModule && !isInternalModuleSupported(n)) return false;
    scalarSet(radio, id, n); return true;
  }
  if (pending && id >= Calib && id < Sticks) {
    unsigned i = (id - Calib) / 9, part = (id - Calib) % 9;
    auto& c = pending->calibration[i];
    c.seen |= 1u << part;
    bool valid = integer(text, part == 0 ? -32768 : part < 3 ? 1 : 0,
                         part < 3 ? 32767 : part == 3 ? XPOTS_MULTIPOS_COUNT - 1 : 255, n);
    if (!valid) c.invalid |= 1u << part;
    else if (part < 3) c.normal[part] = n;
    else c.steps[part - 3] = n;
    return true; // Only the final pot type decides which representation is valid.
  }
  auto& decoded = scalarBuffer; size_t len;
  if (!string(text, decoded, sizeof(decoded) - 1, len)) return false;
  decoded[len] = 0;
  if (id < Source) {
    size_t size = 0; char* dst = stringMember(id - Strings, size, radio);
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
      if (!pending && !isSourceAvailableForBacklightOrVolume(source)) return false;
    }
    if (inverted) source = -source;
    if (id == Source) radio.backlightSrc = source; else radio.volumeSrc = source;
    return true;
  }
  if (id < Sticks) {
    unsigned i = (id - Calib) / 9, part = (id - Calib) % 9;
    auto& c = radio.calib[i];
    if (part >= 3) {
#if XPOTS_MULTIPOS_COUNT > 0
      if (!integer(text, 0, part == 3 ? XPOTS_MULTIPOS_COUNT - 1 : 255, n)) return false;
      auto& steps = c.multipos;
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
    if (!part) { if (len > LEN_ANA_NAME || !isText(text, decoded)) return false; labelSet(context, ADC_INPUT_MAIN, i, decoded, len); }
    else { if (!integer(text, 0, 1, n)) return false; radio.stickInvert = (radio.stickInvert & ~(1u << i)) | (unsigned(n) << i); }
    return true;
  }
  if (id < Switches) {
    unsigned i = (id - Pots) / 3, part = (id - Pots) % 3;
    if (part == 0) { int type = choice(decoded, potTypes, 8); if (type < 0 || !(pending ? type != FLEX_SWITCH || MAX_FLEX_SWITCHES > 0 : isPotTypeAvailable(type))) return false; if (!pending && type == FLEX_MULTIPOS && potType(radio, i) != FLEX_MULTIPOS) {
      auto& c = radio.calib[adcGetInputOffset(ADC_INPUT_FLEX) + i];
      memset(&c, 0, sizeof(c));
    }
    potTypeSet(radio, i, type); }
    else if (part == 1) { if (!integer(text, 0, 1, n)) return false; const unsigned bit = POT_CFG_BITS * i + POT_CFG_TYPE_BITS; radio.potsConfig = (radio.potsConfig & ~(potconfig_t(1) << bit)) | (potconfig_t(n) << bit); }
    else { if (len > LEN_ANA_NAME || !isText(text, decoded)) return false; labelSet(context, ADC_INPUT_FLEX, i, decoded, len); }
    return true;
  }
  if (id < Serial) {
    unsigned i = (id - Switches) / 11, part = (id - Switches) % 11;
    auto& s = radio.switchConfig[i];
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
    if (part) { if (!integer(text, 0, 1, n)) return false; if (pending) {
      const unsigned bit = i * SERIAL_CONF_BITS_PER_PORT + SERIAL_CONF_POWER_BIT;
      radio.serialPort = (radio.serialPort & ~(1u << bit)) | (unsigned(n) << bit);
      pending->powerSeen |= 1u << i;
    } else serialSetPower(i, n); }
    else return serialModeSet(context, i, choice(decoded, uartModes, UART_MODE_COUNT));
    return true;
  }
  if (id < Shortcuts) {
    int channel = !strcmp(decoded, "NONE") ? -1 : analogLookupPhysicalIdx(ADC_INPUT_FLEX, decoded, len);
    if (channel < 0 && strcmp(decoded, "NONE")) return false;
    if (pending) pending->flex[id - Flex] = channel;
    else switchConfigFlex_raw(id - Flex, channel);
    return true;
  }
  if (id < Version) {
#if defined(COLORLCD)
    bool fav = id >= Favorites; unsigned i = id - (fav ? Favorites : Shortcuts);
    bool app = !strncmp(decoded, "APP,", 4);
    int page = app ? int(QM_APP) : choice(decoded, pages, sizeof(pages) / sizeof(*pages));
    if (page < 0) return false;
    const char* tool = app ? decoded + 4 : "";
    if (pending) {
      if (strlen(tool) >= RadioData::ToolNameCapacity) return false;
      strcpy(pending->tools[(fav ? MAX_KEY_SHORTCUTS : 0) + i], tool);
    } else if (!radio.configSetToolName(fav, i, tool)) return false;
    if (fav) radio.qmFavorites[i].shortcut = page;
    else radio.keyShortcuts[i].shortcut = page;
    return true;
#else
    return false;
#endif
  }
  if (id == Version) {
    if (!integer(text, 1, 65535, n)) return false;
    if (pending) pending->version = n; else configVersion = n; return true;
  }
  if (id < LegacySliders) {
    unsigned i = id - Aliases;
    if (i == 0) return set(context, ID_internalModuleBaudrate, text);
    if (i == 1) return set(context, ID_noJitterFilter, text);
    if (i == 2) return set(context, ID_rotEncMode, text);
    const char* modeName = !strncmp(decoded, "MODE_", 5) ? decoded + 5 : decoded;
    int mode = choice(modeName, uartModes, UART_MODE_COUNT);
    return serialModeSet(context, i - 3, mode);
  }
  unsigned i = (id - LegacySliders) / 2, part = (id - LegacySliders) % 2;
  if (part) {
    if (len > LEN_ANA_NAME || !isText(text, decoded)) return false;
    labelSet(context, ADC_INPUT_FLEX, i, decoded, len);
  } else {
    if (!strcmp(decoded, "none")) potTypeSet(radio, i, FLEX_NONE);
    else if (!strcmp(decoded, "with_detent") || !strcmp(decoded, "slider")) potTypeSet(radio, i, FLEX_SLIDER);
    else {
      if (!integer(text, 0, 7, n) || !(pending ? n != FLEX_SWITCH || MAX_FLEX_SWITCHES > 0 : isPotTypeAvailable(n))) return false;
      potTypeSet(radio, i, n);
    }
  }
  return true;
}
static uint8_t seen[(FieldCount + MAX_CALIB_ANALOG_INPUTS + 7) / 8];
enum Section { Root, Calibrations, SticksSection, PotsSection, SwitchesSection, SerialSection, FlexSection, ShortcutsSection, FavoritesSection, SlidersSection,
  Calibration, Stick, Pot, Switch, SerialPort, FlexSwitch, Shortcut, Slider, Steps, OnColor, OffColor };
void enter(void* ctx, const Node& parent, const char* key, const char* text, Node& child, Result& result)
{
  child = parent; child.section = -1;
  int id = -1, index = -1; int64_t number;
  unsigned i = parent.index[0];
  switch (parent.section) {
    case Root: {
      for (unsigned n = 0; n < ScalarCount; ++n) if (!strcmp(key, scalars[n].key)) { id = n; break; }
      for (unsigned n = 0; n < 5; ++n) {
        if (!strcmp(key, stringKeys[n])) id = Strings + n;
        if (!strcmp(key, aliasKeys[n])) id = Aliases + n;
      }
      if (!strcmp(key, "backlightSrc")) id = Source;
      if (!strcmp(key, "volumeSrc")) id = Source + 1;
      if (!strcmp(key, "configVersion")) id = Version;
      const char* names[] = {"calib", "sticksConfig", "potsConfig", "switchConfig", "serialPort", "flexSwitches", "keyShortcuts", "qmFavorites", "slidersConfig"};
      for (unsigned n = 0; n < 9; ++n) if (!strcmp(key, names[n])) child.section = Calibrations + n;
      break;
    }
    case Calibrations: {
      index = adcGetInputIdx(key, strlen(key));
      const char* old[] = {"Rud", "Ele", "Thr", "Ail"};
      if (index < 0) for (unsigned n = 0; n < 4; ++n) if (!strcmp(key, old[n])) index = n;
      if (index < 0 && integer(key, 0, 31, number)) index = number;
      if (index >= 0 && index < adcGetMaxCalibratedInputs()) child.section = Calibration;
      break;
    }
    case SticksSection:
      if (integer(key, 0, adcGetMaxInputs(ADC_INPUT_MAIN) - 1, number)) { index = number; child.section = Stick; }
      break;
    case PotsSection: case SlidersSection:
      index = analogLookupPhysicalIdx(ADC_INPUT_FLEX, key, strlen(key));
      if (index < 0 && parent.section == SlidersSection) {
        const char* legacy = !strcmp(key, "LS") ? "SL1" : !strcmp(key, "RS") ? "SL2" : "";
        index = analogLookupPhysicalIdx(ADC_INPUT_FLEX, legacy, strlen(legacy));
      }
      if (index >= 0) child.section = parent.section == PotsSection ? Pot : Slider;
      break;
    case SwitchesSection:
      index = switchGetIndexFromName(key);
      if (index >= 0 && index < switchGetMaxSwitches()) child.section = Switch;
      break;
    case SerialSection:
      for (unsigned n = 0; n < 3; ++n) if (!strcmp(key, ports[n])) { index = n; child.section = SerialPort; }
      break;
    case FlexSection:
      for (int n = 0; n < MAX_FLEX_SWITCHES; ++n) if (!strcmp(key, switchGetDefaultName(switchGetMaxSwitches() + n))) { index = n; child.section = FlexSwitch; }
      break;
    case ShortcutsSection: case FavoritesSection:
      if (integer(key, 0, parent.section == ShortcutsSection ? 5 : 11, number)) {
        index = (parent.section == ShortcutsSection ? Shortcuts : Favorites) + number; child.section = Shortcut;
      }
      break;
    case Calibration:
      for (unsigned n = 0; n < 4; ++n) if (!strcmp(key, calibKeys[n])) id = Calib + i * 9 + n;
      if (!strcmp(key, "steps")) { child.section = Steps; if (ctx) id = FieldCount + i; }
      break;
    case Steps: if (integer(key, 0, 4, number)) id = Calib + i * 9 + 4 + number; break;
    case Stick: if (!strcmp(key, "name")) id = Sticks + i * 2; if (!strcmp(key, "inv")) id = Sticks + i * 2 + 1; break;
    case Pot: if (!strcmp(key, "type")) id = Pots + i * 3; if (!strcmp(key, "inv")) id = Pots + i * 3 + 1; if (!strcmp(key, "name")) id = Pots + i * 3 + 2; break;
    case Slider: if (!strcmp(key, "type")) id = LegacySliders + i * 2; if (!strcmp(key, "name")) id = LegacySliders + i * 2 + 1; break;
    case Switch:
      for (unsigned n = 0; n < 5; ++n) if (!strcmp(key, switchKeys[n])) id = Switches + i * 11 + n;
      if (!strcmp(key, "onColor")) child.section = OnColor;
      if (!strcmp(key, "offColor")) child.section = OffColor;
      break;
    case OnColor: case OffColor:
      for (unsigned n = 0; n < 3; ++n) if (key[0] == "rgb"[n] && !key[1]) id = Switches + i * 11 + (parent.section == OnColor ? 5 : 8) + n;
      break;
    case SerialPort: if (!strcmp(key, "mode")) id = Serial + i * 2; if (!strcmp(key, "power")) id = Serial + i * 2 + 1; break;
    case FlexSwitch: if (!strcmp(key, "channel")) id = Flex + i; break;
    case Shortcut: if (!strcmp(key, "shortcut")) id = i; break;
  }
  if (index >= 0) child.index[0] = index;
  if (id >= 0) {
    if (!mark(seen, id, sizeof(seen), result)) return;
    auto& f = defaultField; f.reset(); f.metadataOnly = true;
    bool availableField = field(ctx, id, f) && f.available;
    if (ctx && id >= Calib && id < Sticks) availableField = (id - Calib) % 9 < 3 || XPOTS_MULTIPOS_COUNT > 0;
    if (availableField && !set(ctx, id, text)) ++result.invalid;
  } else if (child.section >= 0 && *text) result.error = "expected configuration mapping";
  else if (*text) ++result.unknown;
}
void save(void*, Writer& writer, Field& value)
{
  beginRadioSettingsLoad(); // default baseline in the existing isolated candidate
  auto leaf = [&](unsigned id, const char* key) {
    value.reset(); defaultField.reset();
    if (!field(nullptr, id, value)) { writer.error = "invalid runtime radio value"; return; }
    if (!value.available) return;
    if (!field(&candidate, id, defaultField)) { writer.error = "invalid radio default"; return; }
    if (!defaultField.available || strcmp(value.value, defaultField.value)) writer.value(key, value.value);
  };
  for (unsigned id = 0; id < ScalarCount; ++id) leaf(id, scalars[id].key);
  for (unsigned i = 0; i < 5; ++i) leaf(Strings + i, stringKeys[i]);
  leaf(Source, "backlightSrc"); leaf(Source + 1, "volumeSrc"); leaf(Version, "configVersion");
  writer.begin("calib");
  for (unsigned i = 0; i < adcGetMaxCalibratedInputs(); ++i) {
    writer.begin(adcGetInputName(i));
    for (unsigned p = 0; p < 4; ++p) leaf(Calib + i * 9 + p, calibKeys[p]);
    writer.begin("steps");
    for (unsigned p = 0; p < 5; ++p) { char key[2] = {char('0' + p), 0}; leaf(Calib + i * 9 + 4 + p, key); }
    writer.end(); writer.end();
  }
  writer.end(); writer.begin("sticksConfig");
  for (unsigned i = 0; i < adcGetMaxInputs(ADC_INPUT_MAIN); ++i) {
    writer.begin(i); leaf(Sticks + i * 2, "name"); leaf(Sticks + i * 2 + 1, "inv"); writer.end();
  }
  writer.end(); writer.begin("potsConfig");
  for (unsigned i = 0; i < adcGetMaxInputs(ADC_INPUT_FLEX); ++i) {
    writer.begin(analogGetPhysicalName(ADC_INPUT_FLEX, i));
    leaf(Pots + i * 3, "type"); leaf(Pots + i * 3 + 1, "inv"); leaf(Pots + i * 3 + 2, "name"); writer.end();
  }
  writer.end(); writer.begin("switchConfig");
  for (unsigned i = 0; i < switchGetMaxSwitches(); ++i) {
    writer.begin(switchGetDefaultName(i));
    for (unsigned p = 0; p < 5; ++p) leaf(Switches + i * 11 + p, switchKeys[p]);
    for (unsigned color = 0; color < 2; ++color) {
      writer.begin(color ? "offColor" : "onColor");
      for (unsigned p = 0; p < 3; ++p) { char key[2] = {"rgb"[p], 0}; leaf(Switches + i * 11 + 5 + color * 3 + p, key); }
      writer.end();
    }
    writer.end();
  }
  writer.end(); writer.begin("serialPort");
  for (unsigned i = 0; i < 3; ++i) { writer.begin(ports[i]); leaf(Serial + i * 2, "mode"); leaf(Serial + i * 2 + 1, "power"); writer.end(); }
  writer.end(); writer.begin("flexSwitches");
  for (int i = 0; i < MAX_FLEX_SWITCHES; ++i) { writer.begin(switchGetDefaultName(switchGetMaxSwitches() + i)); leaf(Flex + i, "channel"); writer.end(); }
  writer.end();
  for (unsigned fav = 0; fav < 2; ++fav) {
    writer.begin(fav ? "qmFavorites" : "keyShortcuts");
    for (unsigned i = 0; i < (fav ? 12u : 6u); ++i) { writer.begin(i); leaf((fav ? Favorites : Shortcuts) + i, "shortcut"); writer.end(); }
    writer.end();
  }
}
}
config_stream::Document radioSettingsDocument() { return {nullptr, enter, save, seen, sizeof(seen)}; }

config_stream::Document beginRadioSettingsLoad()
{
  memset(&candidate, 0, sizeof(candidate));
  generalDefault(candidate.radio);
  candidate.version = 1;
  memset(candidate.flex, -1, sizeof(candidate.flex));
  for (unsigned i = 0; i < adcGetMaxCalibratedInputs(); ++i) {
    auto& c = candidate.calibration[i];
    const auto& defaults = candidate.radio.calib[i];
    c.normal[0] = defaults.mid;
    c.normal[1] = defaults.spanNeg;
    c.normal[2] = defaults.spanPos;
#if XPOTS_MULTIPOS_COUNT > 0
    const int pot = int(i) - adcGetInputOffset(ADC_INPUT_FLEX);
    if (pot >= 0 && pot < adcGetMaxInputs(ADC_INPUT_FLEX) &&
        potType(candidate.radio, pot) == FLEX_MULTIPOS) {
      const auto& steps = defaults.multipos;
      c.steps[0] = steps.count < XPOTS_MULTIPOS_COUNT ? steps.count : 0;
      memcpy(c.steps + 1, steps.steps, 5);
    }
#endif
  }
  auto schema = radioSettingsDocument();
  schema.context = &candidate;
  static_assert(FieldCount + MAX_CALIB_ANALOG_INPUTS <= MaxFields, "candidate schema tracking capacity");
  return schema;
}

void resolveRadioSettingsLoad(config_stream::Result& result)
{
  for (unsigned i = 0; i < candidate.modeCount; ++i) {
    const auto& mode = candidate.modes[i];
    if (mode.mode != UART_MODE_NONE && !isSerialModeAvailable(mode.port, mode.mode, candidate.radio))
      ++result.invalid;
    else modeSet(candidate.radio, mode.port, mode.mode);
  }
  unsigned flexCount = 0;
  for (unsigned i = 0; i < adcGetMaxInputs(ADC_INPUT_FLEX); ++i) {
    if (potType(candidate.radio, i) != FLEX_SWITCH) continue;
    if (++flexCount > unsigned(MAX_FLEX_SWITCHES)) {
      const auto defaults = adcGetDefaultPotsConfig();
      potTypeSet(candidate.radio, i, (defaults >> (i * POT_CFG_BITS)) & ((1u << POT_CFG_TYPE_BITS) - 1));
      ++result.invalid;
    }
  }
  for (unsigned i = 0; i < adcGetMaxCalibratedInputs(); ++i) {
    auto& c = candidate.calibration[i];
    auto& target = candidate.radio.calib[i];
    const int pot = int(i) - adcGetInputOffset(ADC_INPUT_FLEX);
    const bool multi = pot >= 0 && pot < adcGetMaxInputs(ADC_INPUT_FLEX) &&
                       potType(candidate.radio, pot) == FLEX_MULTIPOS;
    if (multi && XPOTS_MULTIPOS_COUNT > 0 && c.stepsNotMapping) {
      result.error = "expected configuration mapping";
      return;
    }
    const unsigned mask = multi ? (XPOTS_MULTIPOS_COUNT > 0 ? 0x1f8 : 0) : 7;
    // Omitted calibration components retain their initialized defaults.
    for (unsigned part = 0; part < 9; ++part)
      if (c.invalid & mask & (1u << part)) ++result.invalid;
    if (!multi) {
      target.mid = c.normal[0]; target.spanNeg = c.normal[1]; target.spanPos = c.normal[2];
    }
#if XPOTS_MULTIPOS_COUNT > 0
    else {
      auto& steps = target.multipos;
      steps.count = c.steps[0];
      memcpy(steps.steps, c.steps + 1, 5);
    }
#endif
  }
  for (int i = 0; i < MAX_FLEX_SWITCHES; ++i) {
    const int channel = candidate.flex[i];
    if (channel >= 0 && potType(candidate.radio, channel) != FLEX_SWITCH)
      candidate.flex[i] = -1;
  }
  for (unsigned i = 0; i < 2; ++i) {
    int source = i ? candidate.radio.volumeSrc : candidate.radio.backlightSrc;
    source = abs(source);
    bool valid = true;
    if (source >= MIXSRC_FIRST_POT && source <= MIXSRC_LAST_POT) {
      const unsigned type = potType(candidate.radio, source - MIXSRC_FIRST_POT);
      valid = type != FLEX_NONE && type < FLEX_SWITCH;
    } else if (source >= MIXSRC_FIRST_SWITCH && source <= MIXSRC_LAST_SWITCH) {
      const unsigned sw = source - MIXSRC_FIRST_SWITCH;
      valid = sw < switchGetMaxSwitches() && candidate.radio.switchConfig[sw].type != SWITCH_NONE;
#if defined(FUNCTION_SWITCHES)
      if (switchIsCustomSwitch(sw) && g_model.cfsType(sw) != SWITCH_GLOBAL)
        valid = g_model.cfsType(sw) != SWITCH_NONE;
#endif
    }
    if (!valid) {
      if (i) candidate.radio.volumeSrc = 0; else candidate.radio.backlightSrc = 0;
      ++result.invalid;
    }
  }
}

void commitRadioSettingsLoad()
{
  g_eeGeneral = candidate.radio;
  configVersion = candidate.version;
  for (unsigned type = ADC_INPUT_MAIN; type <= ADC_INPUT_FLEX; ++type)
    for (unsigned i = 0; i < adcGetMaxInputs(type); ++i)
      analogSetCustomLabel(type, i, candidate.labels[adcGetInputOffset(type) + i], LEN_ANA_NAME);
  for (int i = 0; i < MAX_FLEX_SWITCHES; ++i)
    switchConfigFlex_raw(i, candidate.flex[i]);
#if defined(COLORLCD)
  for (unsigned i = 0; i < MAX_KEY_SHORTCUTS; ++i)
    g_eeGeneral.configSetToolName(false, i, candidate.tools[i]);
  for (unsigned i = 0; i < MAX_QM_FAVORITES; ++i)
    g_eeGeneral.configSetToolName(true, i, candidate.tools[MAX_KEY_SHORTCUTS + i]);
#endif
  for (unsigned i = 0; i < MAX_SERIAL_PORTS; ++i)
    if (candidate.powerSeen & (1u << i)) serialSetPower(i, serialGetPower(i));
}
