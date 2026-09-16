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


#include "board.h"

#if defined(EXPORT)
  #define LUA_EXPORT(...)              LEXP(__VA_ARGS__)
  #define LUA_EXPORT_MULTIPLE(...)     LEXP_MULTIPLE(__VA_ARGS__)
  #define LUA_EXPORT_EXTRA(...)        LEXP_EXTRA(__VA_ARGS__)
#else
  #define LUA_EXPORT(...)
  #define LUA_EXPORT_MULTIPLE(...)
  #define LUA_EXPORT_EXTRA(...)
#endif

#define LABELS_LENGTH 100 // Maximum length of the label string
#define LABEL_LENGTH 16

#if defined(COLORLCD)
  #define MAX_MODELS                   60
  #define MAX_OUTPUT_CHANNELS          32 // number of real output channels CH1-CH32
  #define MAX_MIXERS                   64
  #define MAX_EXPOS                    64
  #define MAX_INPUTS                   32
#if defined(STM32H7)
  #define MAX_TELEMETRY_SENSORS        99
#else
  #define MAX_TELEMETRY_SENSORS        60
#endif
  #define MAX_CUSTOM_SCREENS           10
#elif defined(PCBX9DP) || defined(PCBX9E)
  #define MAX_MODELS                   60
  #define MAX_OUTPUT_CHANNELS          32 // number of real output channels CH1-CH32
  #define MAX_MIXERS                   64
  #define MAX_EXPOS                    64
  #define MAX_INPUTS                   32
  #define MAX_TELEMETRY_SENSORS        60
#elif defined(PCBTARANIS)
  #define MAX_MODELS                   60
  #define MAX_OUTPUT_CHANNELS          32 // number of real output channels CH1-CH32
  #define MAX_MIXERS                   64
  #define MAX_EXPOS                    64
  #define MAX_INPUTS                   32
  #define MAX_TELEMETRY_SENSORS        40
#else
  #warning "Unknown board!"
#endif

#define MAX_TIMERS                     3

enum CurveType {
  CURVE_TYPE_STANDARD,
  CURVE_TYPE_CUSTOM,
  CURVE_TYPE_LAST = CURVE_TYPE_CUSTOM
};

#define MIN_POINTS_PER_CURVE           2
#define MAX_POINTS_PER_CURVE           17

#if defined(COLORLCD)
  #define LEN_MODEL_NAME               15
  #define LEN_TIMER_NAME               8
  #define LEN_BITMAP_NAME              14
  #define LEN_EXPOMIX_NAME             6
  #define LEN_CHANNEL_NAME             6
  #define LEN_INPUT_NAME               4
  #define LEN_CURVE_NAME               3
  #define MAX_CURVES                   32
  #define MAX_CURVE_POINTS             512
#elif LCD_W == 212
  #define LEN_MODEL_NAME               12
  #define LEN_TIMER_NAME               8
  #define LEN_BITMAP_NAME              10
  #define LEN_EXPOMIX_NAME             6
  #define LEN_CHANNEL_NAME             6
  #define LEN_INPUT_NAME               4
  #define LEN_CURVE_NAME               3
  #define MAX_CURVES                   32
  #define MAX_CURVE_POINTS             512
#else
  #define LEN_MODEL_NAME               10
  #define LEN_TIMER_NAME               3
  #define LEN_BITMAP_NAME              0
  #define LEN_EXPOMIX_NAME             6
  #define LEN_CHANNEL_NAME             4
  #define LEN_INPUT_NAME               3
  #define LEN_CURVE_NAME               3
  #define MAX_CURVES                   32
  #define MAX_CURVE_POINTS             512
#endif

#define NUM_MODULES                    2

#define XPOTS_MULTIPOS_COUNT           6

#if defined(COLORLCD)
enum MainViews {
  VIEW_BLANK,
  VIEW_TIMERS_ALTITUDE,
  VIEW_CHANNELS,
  VIEW_TELEM1,
  VIEW_TELEM2,
  VIEW_TELEM3,
  VIEW_TELEM4,
  VIEW_COUNT
};
#elif LCD_W >= 212
enum MainViews {
  VIEW_TIMERS,
  VIEW_INPUTS,
  VIEW_COUNT
};
#else
enum MainViews {
  VIEW_OUTPUTS_VALUES,
  VIEW_OUTPUTS_BARS,
  VIEW_INPUTS,
  VIEW_TIMER2,
  VIEW_CHAN_MONITOR,
  VIEW_COUNT
};
#endif

enum BeeperMode {
  e_mode_quiet = -2,
  e_mode_alarms,
  e_mode_nokeys,
  e_mode_all
};

enum ModuleIndex {
  INTERNAL_MODULE,
  EXTERNAL_MODULE,
  MAX_MODULES
};

enum ArmingMode {
  ARMING_MODE_FIRST = 0,
  ARMING_MODE_CH5 = ARMING_MODE_FIRST,
  ARMING_MODE_SWITCH = 1,
  ARMING_MODE_LAST = ARMING_MODE_SWITCH,
};

enum SerialPort {
    SP_AUX1=0,
    SP_AUX2,
    SP_VCP,
    MAX_SERIAL_PORTS,
};

#define MAX_AUX_SERIAL (SP_AUX2 + 1)
#define STORAGE_SERIAL_PORTS 4

// GPS
#define PILOTPOS_MIN_HDOP 500

#if defined(HARDWARE_INTERNAL_MODULE)
#define IS_INTERNAL_MODULE_ENABLED() \
  (g_model.moduleData[INTERNAL_MODULE].type == MODULE_TYPE_CROSSFIRE)
#else
#define IS_INTERNAL_MODULE_ENABLED() (false)
#endif

#if defined(HARDWARE_EXTERNAL_MODULE)
#define IS_EXTERNAL_MODULE_ENABLED() \
  (g_model.moduleData[EXTERNAL_MODULE].type == MODULE_TYPE_CROSSFIRE)
#else
#define IS_EXTERNAL_MODULE_ENABLED() false
#endif

#define IS_MODULE_ENABLED(moduleIdx)                            \
  (g_model.moduleData[moduleIdx].type == MODULE_TYPE_CROSSFIRE)

enum UartModes {
  UART_MODE_NONE,
  UART_MODE_TELEMETRY_MIRROR,
  UART_MODE_RESERVED_TELEMETRY,
  UART_MODE_RESERVED_3,
  UART_MODE_RESERVED_4,
  UART_MODE_LUA,
  UART_MODE_CLI,
  UART_MODE_GPS,
  UART_MODE_DEBUG,
  UART_MODE_SPACEMOUSE,
  UART_MODE_EXT_MODULE,
  UART_MODE_COUNT,
  UART_MODE_MAX = UART_MODE_COUNT-1
};

#define LEN_SWITCH_NAME    3
#define LEN_ANA_NAME       3
#define LEN_MODEL_FILENAME 16
#define LEN_BLUETOOTH_NAME 10

enum TelemetryProtocol {
  PROTOCOL_TELEMETRY_CROSSFIRE = 3,
  PROTOCOL_TELEMETRY_LUA = 15
};

#define TELEM_LABEL_LEN                4
enum TelemetryUnit {
  UNIT_RAW,
  UNIT_VOLTS,
  UNIT_AMPS,
  UNIT_MILLIAMPS,
  UNIT_KTS,
  UNIT_METERS_PER_SECOND,
  UNIT_FEET_PER_SECOND,
  UNIT_KMH,
  UNIT_SPEED = UNIT_KMH,
  UNIT_MPH,
  UNIT_METERS,
  UNIT_DIST = UNIT_METERS,
  UNIT_FEET,
  UNIT_CELSIUS,
  UNIT_TEMPERATURE = UNIT_CELSIUS,
  UNIT_FAHRENHEIT,
  UNIT_PERCENT,
  UNIT_MAH,
  UNIT_WATTS,
  UNIT_MILLIWATTS,
  UNIT_DB,
  UNIT_RPMS,
  UNIT_G,
  UNIT_DEGREE,
  UNIT_RADIANS,
  UNIT_MILLILITERS,
  UNIT_FLOZ,
  UNIT_MILLILITERS_PER_MINUTE,
  UNIT_HERTZ,
  UNIT_MS,
  UNIT_US,
  UNIT_KM,
  UNIT_DBM,
  UNIT_MAX = UNIT_DBM,
  UNIT_SPARE6,
  UNIT_SPARE7,
  UNIT_SPARE8,
  UNIT_SPARE9,
  UNIT_SPARE10,
  UNIT_HOURS,
  UNIT_MINUTES,
  UNIT_SECONDS,
  // FrSky format used for these fields, could be another format in the future
  UNIT_FIRST_VIRTUAL,
  UNIT_CELLS = UNIT_FIRST_VIRTUAL,
  UNIT_DATETIME,
  UNIT_GPS,
  UNIT_BITFIELD,
  UNIT_TEXT,
  // Internal units (not stored in sensor unit)
  UNIT_GPS_LONGITUDE,
  UNIT_GPS_LATITUDE,
  UNIT_DATETIME_YEAR,
  UNIT_DATETIME_DAY_MONTH,
  UNIT_DATETIME_HOUR_MIN,
  UNIT_DATETIME_SEC
};

// TODO: move to stdlcd UI
#if LCD_W >= 212
  #define NUM_LINE_ITEMS 3
#else
  #define NUM_LINE_ITEMS 2
#endif

#if defined(PCBTARANIS)
  #define MAX_TELEM_SCRIPT_INPUTS  8
#endif

enum TelemetryScreenType {
  TELEMETRY_SCREEN_TYPE_NONE,
  TELEMETRY_SCREEN_TYPE_VALUES,
  TELEMETRY_SCREEN_TYPE_BARS,
  TELEMETRY_SCREEN_TYPE_SCRIPT,
#if defined(LUA)
  TELEMETRY_SCREEN_TYPE_MAX = TELEMETRY_SCREEN_TYPE_SCRIPT
#else
  TELEMETRY_SCREEN_TYPE_MAX = TELEMETRY_SCREEN_TYPE_BARS
#endif
};

#define MAX_TELEMETRY_SCREENS 4

#define TELEMETRY_SCREEN_TYPE(screenIndex)                              \
  TelemetryScreenType((g_model.screensType >> (2 * (screenIndex))) & 0x03)

#define IS_BARS_SCREEN(screenIndex)                                     \
  (TELEMETRY_SCREEN_TYPE(screenIndex) == TELEMETRY_SCREEN_TYPE_BARS)

#define LEN_SCRIPT_FILENAME            6

enum PotsWarnMode {
  POTS_WARN_OFF,
  POTS_WARN_MANUAL,
  POTS_WARN_AUTO
};

// Maximum number analog inputs by type
#define MAX_STICKS        4

#if defined(COLORLCD)
  #define MAX_POTS        16
#else
  #define MAX_POTS        8
#endif

#define MAX_VBAT          1
#define MAX_RTC_BAT       1

#define MAX_ANALOG_INPUTS (MAX_STICKS + MAX_POTS + MAX_VBAT + MAX_RTC_BAT)
#define MAX_CALIB_ANALOG_INPUTS (MAX_STICKS + MAX_POTS)

#define MAX_SWITCHES      20

#if !defined(MAX_FLEX_SWITCHES)
#define MAX_FLEX_SWITCHES 0
#endif

#if NUM_TRIMS > 6
#define MAX_TRIMS 8
#else
#define MAX_TRIMS 6
#endif

#define MAX_XPOTS_POSITIONS (MAX_POTS * XPOTS_MULTIPOS_COUNT)

enum SwitchSources {
  SWSRC_NONE = 0,

  SWSRC_FIRST_SWITCH,
  SWSRC_LAST_SWITCH = SWSRC_FIRST_SWITCH + (MAX_SWITCHES * 3) - 1,

  SWSRC_FIRST_MULTIPOS_SWITCH,
  SWSRC_LAST_MULTIPOS_SWITCH = SWSRC_FIRST_MULTIPOS_SWITCH + MAX_XPOTS_POSITIONS - 1,

  SWSRC_FIRST_TRIM,
  SWSRC_LAST_TRIM = SWSRC_FIRST_TRIM + 2 * MAX_TRIMS - 1,

  SWSRC_ON,
  SWSRC_ONE,

  SWSRC_TELEMETRY_STREAMING,

  SWSRC_FIRST_SENSOR,
  SWSRC_LAST_SENSOR = SWSRC_FIRST_SENSOR+MAX_TELEMETRY_SENSORS-1,

  SWSRC_RADIO_ACTIVITY,

#if defined(DEBUG_LATENCY)
  SWSRC_LATENCY_TOGGLE,
#endif

  SWSRC_COUNT,

  SWSRC_OFF = -SWSRC_ON,

  SWSRC_LAST = SWSRC_COUNT-1,
  SWSRC_FIRST = -SWSRC_LAST,

  SWSRC_INVERT = SWSRC_COUNT+1,
};

enum SwitchTypes {
  SW_SWITCH = 1 << 0,
  SW_TRIM = 1 << 1,
  SW_TELEM = 1 << 4,
  SW_OTHER = 1 << 5,
  SW_NONE = 1 << 20,
};

enum MixSources {
  MIXSRC_NONE,

  MIXSRC_FIRST,
  MIXSRC_FIRST_INPUT = MIXSRC_FIRST,
  MIXSRC_LAST_INPUT = MIXSRC_FIRST_INPUT + MAX_INPUTS - 1,


  // Semantic sticks
  MIXSRC_FIRST_STICK,
  MIXSRC_LAST_STICK = MIXSRC_FIRST_STICK + MAX_STICKS - 1,

  MIXSRC_FIRST_POT,
  MIXSRC_LAST_POT = MIXSRC_FIRST_POT + MAX_POTS - 1,

#if defined(IMU)
  MIXSRC_TILT_X,
  MIXSRC_TILT_Y,
#endif

#if defined(PCBHORUS)
  MIXSRC_FIRST_SPACEMOUSE,
  MIXSRC_SPACEMOUSE_A = MIXSRC_FIRST_SPACEMOUSE,
  MIXSRC_SPACEMOUSE_B,
  MIXSRC_SPACEMOUSE_C,
  MIXSRC_SPACEMOUSE_D,
  MIXSRC_SPACEMOUSE_E,
  MIXSRC_SPACEMOUSE_F,
  MIXSRC_LAST_SPACEMOUSE = MIXSRC_SPACEMOUSE_F,
#endif

  MIXSRC_MIN,
  MIXSRC_MAX,

#if defined(LUMINOSITY_SENSOR)
  MIXSRC_LIGHT,
#endif

  MIXSRC_FIRST_RESERVED_TRIM,
  MIXSRC_LAST_RESERVED_TRIM = MIXSRC_FIRST_RESERVED_TRIM + MAX_TRIMS - 1,

  MIXSRC_FIRST_SWITCH,
  MIXSRC_LAST_SWITCH = MIXSRC_FIRST_SWITCH + MAX_SWITCHES - 1,

#if defined(FUNCTION_SWITCHES)
  MIXSRC_FIRST_CUSTOMSWITCH_GROUP,
  MIXSRC_LAST_CUSTOMSWITCH_GROUP = MIXSRC_FIRST_CUSTOMSWITCH_GROUP + NUM_FUNCTIONS_GROUPS - 1,
#endif

  MIXSRC_FIRST_CH,
  MIXSRC_LAST_CH = MIXSRC_FIRST_CH + MAX_OUTPUT_CHANNELS - 1,

  MIXSRC_TX_VOLTAGE,
  MIXSRC_TX_TIME,
  MIXSRC_TX_GPS,

  MIXSRC_FIRST_TIMER,
  MIXSRC_LAST_TIMER = MIXSRC_FIRST_TIMER + MAX_TIMERS - 1,

  MIXSRC_FIRST_TELEM,
  MIXSRC_LAST_TELEM = MIXSRC_FIRST_TELEM + 3 * MAX_TELEMETRY_SENSORS - 1,

  MIXSRC_INVERT,
};

#define MIXSRC_LAST                 MIXSRC_LAST_CH
#define INPUTSRC_FIRST              MIXSRC_FIRST_STICK
#define INPUTSRC_LAST               MIXSRC_LAST_TIMER

#if defined(FUNCTION_SWITCHES)
#define MIXSRC_LAST_REGULAR_SWITCH  (MIXSRC_FIRST_SWITCH + switchGetMaxAllSwitches() - 1)
#define MIXSRC_FIRST_FS_SWITCH      (MIXSRC_LAST_REGULAR_SWITCH + 1)
#endif

constexpr int16_t MIXSRC_MAX_VALUE = 30000;

enum SrcTypes {
  SRC_INPUT = 1 << 0,
  SRC_STICK = 1 << 2,
  SRC_POT = 1 << 3,
  SRC_TILT = 1 << 4,
  SRC_SPACEMOUSE = 1 << 5,
  SRC_MINMAX = 1 << 6,
  SRC_SWITCH = 1 << 9,
  SRC_FUNC_SWITCH = 1 << 10,
  SRC_CHANNEL = 1 << 13,
  SRC_TX = 1 << 16,
  SRC_TIMER = 1 << 17,
  SRC_TELEM = 1 << 18,
  SRC_LIGHT = 1 << 19,
  SRC_NONE = 1 << 30,
  SRC_INVERT = 1 << 31,
};

enum BacklightMode {
  e_backlight_mode_off  = 0,
  e_backlight_mode_keys = 1,
  e_backlight_mode_sticks = 2,
  e_backlight_mode_all = e_backlight_mode_keys+e_backlight_mode_sticks,
  e_backlight_mode_on
};

enum TimerModes {
  TMRMODE_OFF,
  TMRMODE_ON,
  TMRMODE_START,
  TMRMODE_THR,
  TMRMODE_THR_REL,
  TMRMODE_THR_START,
  TMRMODE_COUNT,
  TMRMODE_MAX = TMRMODE_COUNT - 1
};

enum CountDownModes {
  COUNTDOWN_SILENT,
  COUNTDOWN_BEEPS,
  COUNTDOWN_VOICE,
  COUNTDOWN_NON_HAPTIC_LAST = COUNTDOWN_VOICE,
#if defined(HAPTIC)
  COUNTDOWN_HAPTIC,
  COUNTDOWN_BEEPS_AND_HAPTIC,
  COUNTDOWN_VOICE_AND_HPTIC,
#endif
  COUNTDOWN_COUNT
};

enum BluetoothModes {
  BLUETOOTH_OFF,
  BLUETOOTH_TELEMETRY,
  BLUETOOTH_MAX = BLUETOOTH_TELEMETRY
};

enum HatsMode {
  HATSMODE_TRIMS_ONLY,
  HATSMODE_KEYS_ONLY,
  HATSMODE_SWITCHABLE,
  HATSMODE_GLOBAL
};

#if defined(STM32F4)
enum UartSampleModes {
  UART_SAMPLE_MODE_NORMAL = 0,
  UART_SAMPLE_MODE_ONEBIT,

  UART_SAMPLE_MODE_MAX = UART_SAMPLE_MODE_ONEBIT
};
#endif

// A model On/Off setting that can also take a global value
enum ModelOverridableEnable {
  OVERRIDE_GLOBAL,
  OVERRIDE_OFF,
  OVERRIDE_ON
};

#define SELECTED_THEME_NAME_LEN 26

// PPM Units
enum PPMUnit {
    PPM_PERCENT_PREC0,
    PPM_PERCENT_PREC1,
    PPM_US
};

constexpr int32_t MIX_WEIGHT_MAX = 500;
constexpr int32_t MIX_WEIGHT_MIN = -500;
constexpr int32_t MIX_OFFSET_MAX = 500;
constexpr int32_t MIX_OFFSET_MIN = -500;
