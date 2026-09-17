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

#pragma once

#include <inttypes.h>
#include "board.h"
#include "dataconstants.h"
#include "definitions.h"
#include "edgetx_types.h"
#include "globals.h"
#include "serial.h"
#include "usb_joystick.h"
#include "input_mapping.h"
#include "debug.h"
#include "bitfield.h"

#if defined(COLORLCD)
#include "datastructs_screen.h"
#include "quick_menu_def.h"
#endif

#if defined(PCBTARANIS)
  #define N_TARANIS_FIELD(x)
  #define TARANIS_FIELD(x) x;
#else
  #define N_TARANIS_FIELD(x) x;
  #define TARANIS_FIELD(x)
#endif

#if defined(PCBX9E)
  #define TARANIS_PCBX9E_FIELD(x)       x;
#else
  #define TARANIS_PCBX9E_FIELD(x)
#endif

#if defined(PCBHORUS)
  #define N_HORUS_FIELD(x)
  #define HORUS_FIELD(x) x;
#else
  #define N_HORUS_FIELD(x) x;
  #define HORUS_FIELD(x)
#endif

/*
 * Mixer structure
 */

PACK(struct MixData {
  uint16_t destCh:5;
  int16_t  srcRaw:10; // srcRaw=0 means not used
  uint16_t reservedTrimCarry:1;
  uint16_t spare:2;
  int32_t weight:11;
  int32_t offset:11;
  char name[LEN_MIX_NAME];
});

/*
 * Timer structure
 */

PACK(struct TimerData {
  uint32_t start:22;
  int32_t  swtch:10;
  int32_t  value:22;
  uint32_t mode:3;
  uint32_t countdownBeep:2;
  uint32_t minuteBeep:1;
  uint32_t persistent:2;
  int32_t  countdownStart:2;
  uint8_t  showElapsed:1;
  uint8_t  extraHaptic:1;
  uint8_t  spare:6;
  char name[LEN_TIMER_NAME];
});

PACK(struct RFAlarmData {
  int8_t warning;
  int8_t critical;
});

typedef int16_t telemetry_value_t;

#if !defined(COLORLCD)
PACK(struct TelemetryBarData {
  source_t source;
  telemetry_value_t barMin;           // minimum for bar display
  telemetry_value_t barMax;           // ditto for max display (would usually = ratio)
});

PACK(struct TelemetryLineData {
  source_t sources[NUM_LINE_ITEMS];
});

#if defined(PCBTARANIS)
PACK(struct TelemetryScriptData {
  char    file[LEN_SCRIPT_FILENAME];
  int16_t inputs[MAX_TELEM_SCRIPT_INPUTS];
});
#endif

union TelemetryScreenData {
  TelemetryBarData  bars[4];
  TelemetryLineData lines[4];
#if defined(PCBTARANIS)
  TelemetryScriptData script;
#endif
};

#endif

PACK(struct VarioData {
  uint8_t source:7; // telemetry sensor idx + 1
  uint8_t centerSilent:1;
  int8_t  centerMax;
  int8_t  centerMin;
  int8_t  min;
  int8_t  max;
});

/*
 * Telemetry Sensor structure
 */

#define TELEMETRY_ENDPOINT_NONE    0xFF
#define TELEMETRY_ENDPOINT_SPORT   0x07

PACK(struct TelemetrySensor {
  union {
    uint16_t id;  // Telemetry data identifier; source unit is derived from type.
    uint16_t persistentValue;
  };
  union {
    uint8_t instance;
    uint8_t formula;
  };
  char label[TELEM_LABEL_LEN]; // user defined label
  uint8_t  subId;
  uint8_t  type:1; // 0=custom / 1=calculated
                   // user can choose what unit to display each value in
  uint8_t  spare1:1;
  uint8_t  unit:6;
  uint8_t  prec:2;
  uint8_t  autoOffset:1;
  uint8_t  filter:1;
  uint8_t  logs:1;
  uint8_t  persistent:1;
  uint8_t  onlyPositive:1;
  uint8_t  spare2:1;
  union {
    PACK(struct {
      uint16_t ratio;
      int16_t  offset;
    }) custom;
    PACK(struct {
      uint8_t source;
      uint8_t index;
      uint16_t spare;
    }) cell;
    PACK(struct {
      int8_t sources[4];
    }) calc;
    PACK(struct {
      uint8_t source;
      uint8_t spare[3];
    }) consumption;
    PACK(struct {
      uint8_t gps;
      uint8_t alt;
      uint16_t spare;
    }) dist;
    uint32_t param;
  };

    void init(const char *label, uint8_t unit=UNIT_RAW, uint8_t prec=0);
    void init(uint16_t id);
    bool isAvailable() const;
    int32_t getValue(int32_t value, uint8_t unit, uint8_t prec) const;
    bool isConfigurable() const;
    bool isPrecConfigurable() const;
    int32_t getPrecMultiplier() const;
    int32_t getPrecDivisor() const;
    bool isSameInstance(TelemetryProtocol protocol, uint8_t instance);
  ;
});

/*
 * Module structure
 */

PACK(struct ModuleData {
  // Runtime RF state retains the same shape on boards without an external antenna.
  uint8_t type:6;
  int8_t  antennaMode:2;
  uint8_t channelsStart;
  int8_t  channelsCount; // 0=8 channels

  union {
    PACK(struct {
      uint8_t telemetryBaudrate:3;
      uint8_t crsfArmingMode:1;
      uint8_t spare2:4;
      int16_t crsfArmingTrigger:10;
      int16_t spare3:6;
    }) crsf;
  };

  inline uint8_t getChannelsCount() const
  {
    return channelsCount + 8;
  }
});

/*
 * Model structure
 */

#if LEN_BITMAP_NAME > 0
#define MODEL_HEADER_BITMAP_FIELD      char bitmap[LEN_BITMAP_NAME];
#else
#define MODEL_HEADER_BITMAP_FIELD
#endif

PACK(struct ModelHeader {
  char      name[LEN_MODEL_NAME]; // must be first for eeLoadModelName
  uint8_t   modelId[NUM_MODULES];
  MODEL_HEADER_BITMAP_FIELD
#if defined(STORAGE_MODELSLIST)
  char      labels[LABELS_LENGTH];
#endif
});

// 2 bits per switch, max 32 switches
static_assert(sizeof(swconfig_t) >= (MAX_SWITCHES * 2 + 7) / 8,
              "MAX_SWITCHES must fit swconfig_t");

static_assert(sizeof(swarnstate_t) >= (MAX_SWITCHES * 2 + 7) / 8,
              "MAX_SWITCHES must fit swarnstate_t");

// pot config: 4 bits per pot
static_assert(sizeof(potconfig_t) * 8 >= ((MAX_POTS - 1) / 4) + 1,
              "MAX_POTS must fit potconfig_t");

// pot warning enabled: 1 bit per pot
static_assert(sizeof(potwarnen_t) * 8 >= MAX_POTS,
              "MAX_POTS must fit potwarnen_t");

#if defined(PCBX9DP) || defined(PCBX9E)
  // telemetry sensor idx + 1
  #define TOPBAR_DATA \
    uint8_t voltsSource; \
    uint8_t altitudeSource;
#else
  #define TOPBAR_DATA
#endif

struct RGBLedColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  uint32_t getColor() {
    return ((r << 16) + (g << 8) + b);
  }

  void setColor(uint32_t color) {
    r = color >> 16;
    g = color >> 8;
    b = color;
  }
};

#if defined(FUNCTION_SWITCHES)
enum booleanEnum {
  BOOL_OFF,
  BOOL_ON
};

PACK(struct customSwitch {

  char name[LEN_SWITCH_NAME];
  uint8_t type:3;
#if NUM_FUNCTIONS_GROUPS > 3
  uint8_t group:3;
#else
  uint8_t group:2;
#endif
  uint8_t start:2;
  uint8_t state:1;
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
  uint8_t onColorLuaOverride:1;
  uint8_t offColorLuaOverride:1;
#if NUM_FUNCTIONS_GROUPS > 3
  uint8_t spare:5;
#else
  uint8_t spare:6;
#endif
  RGBLedColor onColor;
  RGBLedColor offColor;
#else
#if NUM_FUNCTIONS_GROUPS > 3
  uint8_t spare:7;
#endif
#endif
});
#endif

#if defined(FUNCTION_SWITCHES)
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
  #define FUNCTION_SWITCHS_RGB_LEDS_FIELDS \

#else
  #define FUNCTION_SWITCHS_RGB_LEDS_FIELDS
#endif
  #define FUNCTION_SWITCHS_FIELDS \
    FUNCTION_SWITCHS_RGB_LEDS_FIELDS \
    customSwitch customSwitches[NUM_FUNCTIONS_SWITCHES]; \
    uint8_t cfsGroupOn;
#else
  #define FUNCTION_SWITCHS_FIELDS
#endif

/*
 * USB Joystick channel structure
 */

PACK(struct USBJoystickChData {
  uint8_t mode:3;
  uint8_t inversion:1;
  uint8_t param:4;
  uint8_t btn_num:5;
  uint8_t switch_npos:3;

#if defined(USBJ_EX)

    uint8_t btnCount() {
      // Use one less joystick button for 2POS and 3POS switches for Companion mode
      if ((param == USBJOYS_BTN_MODE_COMPANION) && (switch_npos > 0) && (switch_npos < 3))
        return switch_npos;
      return switch_npos + 1;
    }
    uint8_t lastBtnNumNoCLip() {
      uint8_t last = btn_num + switch_npos;
      // Use one less joystick button for 2POS and 3POS switches for Companion mode
      if ((param == USBJOYS_BTN_MODE_COMPANION) && (switch_npos > 0) && (switch_npos < 3)) last -= 1;
      return last;
    }
    uint8_t lastBtnNum() {
      uint8_t last = lastBtnNumNoCLip();
      if (last >= USBJ_BUTTON_SIZE) {
        last = USBJ_BUTTON_SIZE - 1;
      }
      return last;
    }
  ;
#endif
});

PACK(struct ModelData {

  ModelHeader header;

  TimerData timers[MAX_TIMERS];
  uint8_t   telemetryProtocol:3;
  uint8_t   reservedThrTrim:1;
  // Retained runtime padding; it is no longer part of a storage layout contract.
  uint8_t   spareModelFlags:1;
  uint8_t   reservedDisplayTrims:2;
  uint8_t   ignoreSensorIds:1;
  int8_t    reservedTrimInc:3;
  uint8_t   disableThrottleWarning:1;
  uint8_t   displayChecklist:1;
  uint8_t   reservedExtendedTrims:1;
  uint8_t   throttleReversed:1;
  uint8_t   enableCustomThrottleWarning:1;
  uint8_t   disableTelemetryWarning:1;
  uint8_t   showInstanceIds:1;
  uint8_t   checklistInteractive:1;
#if defined(USE_HATS_AS_KEYS)
  uint8_t hatsMode:2;
  uint8_t   spare3:2;  // padding to 8-bit aligment
#else
  uint8_t   spare3:4;  // padding to 8-bit aligment
#endif
  int8_t    customThrottleWarningPosition;
  BeepANACenter beepANACenter;
  MixData   mixData[MAX_MIXERS];

  uint8_t thrTraceSrc;

  swarnstate_t switchWarning;

  VarioData varioData;

  TOPBAR_DATA

  RFAlarmData rfAlarms;

  uint8_t reservedThrTrimSw:3;
  uint8_t potsWarnMode:2;
  uint8_t jitterFilter:2;
  uint8_t spare1:1;

  ModuleData moduleData[NUM_MODULES];
  potwarnen_t potsWarnEnabled;
  int8_t potsWarnPosition[MAX_POTS];

  TelemetrySensor telemetrySensors[MAX_TELEMETRY_SENSORS];

  TARANIS_PCBX9E_FIELD(uint8_t toplcdTimer)

#if defined(COLORLCD)
  uint8_t topbarWidgetWidth[MAX_TOPBAR_ZONES];
  uint8_t view;

  void resetScreenData();
  const char* getScreenLayoutId(int screenNum);
  void setScreenLayoutId(int screenNum, const char* s);
  TopBarPersistentData* getTopbarData();
  bool hasScreenData(int screenNum);
  CustomScreenData* getScreenData(int screenNum);
  LayoutPersistentData* getScreenLayoutData(int screenNum);
  WidgetPersistentData* getWidgetData(int screenNum, int zoneNum);
  void removeScreenLayout(int idx);
#else
  uint8_t screensType; /* 2bits per screen (None/Gauges/Numbers/Script) */
  TelemetryScreenData screens[MAX_TELEMETRY_SCREENS];
  uint8_t getTelemetryScreenType(unsigned screen) const { return bfGet<uint8_t>(screensType, screen * 2, 2); }
  void setTelemetryScreenType(unsigned screen, uint8_t type) { screensType = bfSet<uint8_t>(screensType, type, screen * 2, 2); }
  uint8_t view;
#endif

  FUNCTION_SWITCHS_FIELDS

  uint8_t usbJoystickExtMode:1;
  uint8_t usbJoystickIfMode:3;
  uint8_t usbJoystickCircularCut:4;
  USBJoystickChData usbJoystickCh[USBJ_MAX_JOYSTICK_CHANNELS];

  // Radio level tabs control (model settings)
#if defined(COLORLCD)
  uint8_t radioThemesDisabled:2;
#endif
  uint8_t spareViewOption:2;
  // Model level tabs control (model setting)
  uint8_t reservedModelFeature:2;
  uint8_t reservedVariableFeature:2;
  uint8_t reservedConditionFeature:2;
  uint8_t modelTelemetryDisabled:2;

  SwitchConfig getSwitchType(uint8_t n);
  void setSwitchType(uint8_t n, SwitchConfig v);
  char* getSwitchCustomName(uint8_t n);
  bool switchHasCustomName(uint8_t n);

  uint8_t getSwitchStateForWarning(uint8_t n);
  uint8_t getSwitchWarning(uint8_t n) { return bfGet<swarnstate_t>(switchWarning, n * 2, 2); }
  void setSwitchWarning(uint8_t n, uint8_t v) { switchWarning = bfSet<swarnstate_t>(switchWarning, v, n * 2, 2); }

#if defined(FUNCTION_SWITCHES)
  uint8_t getSwitchGroup(uint8_t n);
  fsStartPositionType getSwitchStart(uint8_t n);
  void setSwitchStart(uint8_t n, fsStartPositionType v);
  SwitchConfig cfsType(uint8_t n);
  void cfsSetType(uint8_t n, SwitchConfig v);
  char* cfsName(uint8_t n);
  uint8_t cfsGroup(uint8_t n);
  void cfsSetGroup(uint8_t n, uint8_t v);
  fsStartPositionType cfsStart(uint8_t n);
  void cfsSetStart(uint8_t n, fsStartPositionType v);
  bool cfsState(uint8_t n);
  void cfsSetState(uint8_t n, bool v);
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
  RGBLedColor& getSwitchOnColor(uint8_t n);
  RGBLedColor& getSwitchOffColor(uint8_t n);
  bool getSwitchOnColorLuaOverride(uint8_t n);
  bool getSwitchOffColorLuaOverride(uint8_t n);
  RGBLedColor& cfsOnColor(uint8_t n);
  RGBLedColor& cfsOffColor(uint8_t n);
  bool cfsOnColorLuaOverride(uint8_t n);
  bool cfsOffColorLuaOverride(uint8_t n);
  void cfsSetOnColorLuaOverride(uint8_t n, bool v);
  void cfsSetOffColorLuaOverride(uint8_t n, bool v);
#endif
  bool cfsGroupAlwaysOn(uint8_t n) { return bfGet<uint8_t>(cfsGroupOn, n, 1); }
  void cfsSetGroupAlwaysOn(uint8_t n, bool v) { cfsGroupOn = bfSet<uint8_t>(cfsGroupOn, v, n, 1); }
#endif
});

/*
 * Radio structure
 */

#if XPOTS_MULTIPOS_COUNT > 0
PACK(struct StepsCalibData {
  uint8_t count;
  uint8_t steps[XPOTS_MULTIPOS_COUNT-1];
});
#endif

// Continuous and multiposition calibration share runtime storage. The named
// union member lets semantic storage use fields without casting byte layouts.
PACK(struct CalibData {
  union {
    struct {
      int16_t mid;
      int16_t spanNeg;
      int16_t spanPos;
    };
#if XPOTS_MULTIPOS_COUNT > 0
    StepsCalibData multipos;
#endif
  };
});

#if defined(COLORLCD)
  #define EXTRA_GENERAL_FIELDS \
    char currModelFilename[LEN_MODEL_FILENAME+1]; \
    uint8_t blOffBright; \
    char bluetoothName[LEN_BLUETOOTH_NAME];
#else
  #define EXTRA_GENERAL_FIELDS \
    uint8_t  backlightColor; \
    char bluetoothName[LEN_BLUETOOTH_NAME];
#endif

PACK(struct switchDef {

  char name[LEN_SWITCH_NAME];
  uint8_t type:3;
#if defined(FUNCTION_SWITCHES)
  uint8_t start:2;
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
  uint8_t onColorLuaOverride:1;
  uint8_t offColorLuaOverride:1;
  uint8_t spare:1;
  RGBLedColor onColor;
  RGBLedColor offColor;
#else
  uint8_t spare:3;
#endif
#else
  uint8_t spare:5;
#endif
});

#if defined(COLORLCD)
#define MAX_KEY_SHORTCUTS 6
#define MAX_QM_FAVORITES 12
PACK(struct KeyShortcut {

  uint8_t shortcut;
});
PACK(struct QMFavorite {

  uint8_t shortcut;
});
#endif

PACK(struct RadioData {

  // Real attributes
  uint8_t manuallyEdited:1;
  int8_t timezoneMinutes:3;    // -3 to +3 ==> (-45 to 45 minutes in 15 minute increments)
  uint8_t ppmunit:2;  // PPMUnit enum
#if defined(USE_HATS_AS_KEYS)
  uint8_t hatsMode:2;
#else
  uint8_t hatsModeSpare:2;
#endif

  CalibData calib[MAX_CALIB_ANALOG_INPUTS];
  uint16_t chkSum;
  N_HORUS_FIELD(int8_t currModel);
  N_HORUS_FIELD(uint8_t contrast);
  uint8_t vBatWarn;
  int8_t txVoltageCalibration;
  uint8_t backlightMode:3;
  int8_t antennaMode:2;
  uint8_t disableRtcWarning:1;
  uint8_t keysBacklight:1;
  uint8_t dontPlayHello:1;
  uint8_t internalModule;
  uint8_t view;            // index of view in main screen
  int8_t buzzerModeSkip:2; // 2 bits for alignment
  uint8_t fai:1;
  int8_t beepMode:2;
  uint8_t alarmsFlash:1;
  uint8_t disableMemoryWarning:1;
  uint8_t disableAlarmWarning:1;
  uint8_t stickMode:2;
  int8_t timezone:5;
  uint8_t adjustRTC:1;
  uint8_t inactivityTimer;

  uint8_t internalModuleBaudrate:3;
  int8_t splashMode:3; /* 3bits */
  int8_t hapticMode:2;
  int8_t switchesDelay;
  uint8_t lightAutoOff;
  uint8_t templateSetup;   // RETA order for receiver channels
  int8_t hapticLength;
  int8_t beepLength:3;
  int8_t hapticStrength:3;
  uint8_t gpsFormat:1;
  uint8_t  audioMuteEnable:1;
  uint8_t speakerPitch;
  int8_t speakerVolume;
  int8_t vBatMin;
  int8_t vBatMax;

  uint8_t  backlightBright;
  uint32_t globalTimer;
  uint8_t  bluetoothBaudrate:4;
  uint8_t  bluetoothMode:4;

  uint8_t  countryCode:2;
  int8_t   pwrOnSpeed:3;
  int8_t   pwrOffSpeed:3;

  uint8_t  noJitterFilter:1; /* 0 - Jitter filter active */
  uint8_t  imperial:1;
  uint8_t  disableRssiPoweroffAlarm:1;
  uint8_t  USBMode:2;
  uint8_t  spareJackMode:2;
  uint8_t  reservedAccessoryPower:1;

  char     ttsLanguage[2];
  char     uiLanguage[2];
  int8_t   beepVolume:4;
  int8_t   wavVolume:4;
  int8_t   varioVolume:4;
  int8_t   backgroundVolume:4;
  int8_t   varioPitch;
  int8_t   varioRange;
  int8_t   varioRepeat;

  uint32_t serialPort;

  uint8_t stickInvert;
  potconfig_t potsConfig;
  switchDef switchConfig[MAX_SWITCHES];

  EXTRA_GENERAL_FIELDS

  uint8_t  rotEncMode:3;

#if defined(STM32F4)
  int8_t uartSampleMode:2; // See UartSampleModes
#else
  uint8_t uartSampleModeSpare:2;
#endif

#if defined(STICK_DEAD_ZONE)
  uint8_t  stickDeadZone:3;
#else
  uint8_t  stickDeadZoneSpare:3;
#endif

#if defined(IMU)
  int8_t imuMax;
  int8_t imuOffset;
  uint8_t imuInvert;  // user inversion, default off; XORed with hal.h IMU_INVERT_X/Y at apply time (bit0=X, bit1=Y)
#endif

#if defined(COLORLCD)
  char selectedTheme[SELECTED_THEME_NAME_LEN];
#endif

  int16_t backlightSrc:10;

  int16_t spareRadioViewOption:1;
  int16_t reservedModelFeature:1;
  int16_t reservedVariableFeature:1;

  int16_t volumeSrc:10;

  int16_t reservedConditionFeature:1;
  int16_t modelTelemetryDisabled:1;
  int16_t sparePoweroffAlarm:1;
  int16_t disablePwrOnOffHaptic:1;

  uint8_t modelQuickSelect:1;
  uint8_t oneLogPerDay:1;
  uint8_t keyLockEnabled:1;

#if defined(COLORLCD)
  uint8_t labelSingleSelect:1;  // 0 = multi-select, 1 = single select labels
  uint8_t labelMultiMode:1;     // 0 = match all labels (AND), 1 = match any labels (OR)
  uint8_t favMultiMode:1;       // 0 = match all (AND), 1 = match any (OR)
  // Radio level tabs control (global settings)
  uint8_t modelSelectLayout:2;
  uint8_t radioThemesDisabled:1;
#if defined(USB_CHARGE_CONTROL)
  // 0 = charge while USB active (default), 1 = hold the charger off while USB
  // is plugged in SD/Joystick/VCP mode
  uint8_t usbChargeDisabled:1;
  uint8_t spare:6;
#else
  uint8_t spare:7;
#endif
#elif LCD_W == 128
  uint8_t invertLCD:1;          // Invert B&W LCD display
  uint8_t spare:4;
#else
  uint8_t spare:5;
#endif

  uint8_t pwrOffIfInactive;

#if defined(COLORLCD)
  KeyShortcut keyShortcuts[MAX_KEY_SHORTCUTS];
  QMFavorite qmFavorites[MAX_QM_FAVORITES];
#endif

  uint8_t getBrightness() const
  {
#if OLED_SCREEN
    return contrast;
#else
    return backlightBright;
#endif
  };

  char* getSwitchCustomName(uint8_t n);
  bool switchHasCustomName(uint8_t n);
  SwitchConfig switchType(uint8_t n);
  void switchSetType(uint8_t n, SwitchConfig v);
  char* switchName(uint8_t n);
#if defined(FUNCTION_SWITCHES)
  uint8_t switchGroup(uint8_t n) { return 0; }
  fsStartPositionType switchStart(uint8_t n);
  void switchSetStart(uint8_t n, fsStartPositionType v);
#if defined(FUNCTION_SWITCHES_RGB_LEDS)
  RGBLedColor& switchOnColor(uint8_t n);
  RGBLedColor& switchOffColor(uint8_t n);
  bool cfsOnColorLuaOverride(uint8_t n);
  bool cfsOffColorLuaOverride(uint8_t n);
  void cfsSetOnColorLuaOverride(uint8_t n, bool v);
  void cfsSetOffColorLuaOverride(uint8_t n, bool v);
#endif
#endif

#if defined(COLORLCD)
  int getKeyShortcutNum(event_t event);
  event_t getKeyShortcutEvent(int n);
  QMPage getKeyShortcut(event_t event);
  bool hasKeyShortcut(QMPage shortcut, event_t event);
  void setKeyShortcut(event_t event, QMPage shortcut);
  void defaultKeyShortcuts();
  void setKeyToolName(event_t event, const std::string name);
  const std::string getKeyToolName(event_t event);
  void setFavoriteToolName(int fav, const std::string name);
  const std::string getFavoriteToolName(int fav);
  // Bounded, allocation-free access for the configuration adapter.
  static constexpr unsigned ToolNameCapacity = 64;
  const char* configToolName(bool favorite, unsigned index) const;
  bool configSetToolName(bool favorite, unsigned index, const char* name);
#endif
});

#undef SWITCHES_WARNING_DATA
#undef TELEMETRY_DATA
#undef SCRIPTS_DATA
#undef CUSTOM_SCREENS_DATA
#undef EXTRA_GENERAL_FIELDS
