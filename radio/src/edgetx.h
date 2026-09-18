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

#include <stdlib.h>
#include "definitions.h"
#include "edgetx_types.h"
#include "edgetx_helpers.h"
#include "touch.h"
#include "switches.h"
#include "board.h"

#if !defined(SIMU)
#include "usbd_msc_conf.h"
#endif

#if !defined(COLORLCD)
  #include "lib_file.h"
#endif

#if defined(FAI)
  #define IS_FAI_ENABLED() true
  #define IF_FAI_CHOICE(x)
#elif defined(FAI_CHOICE)
  #define IS_FAI_ENABLED() g_eeGeneral.fai
  #define IF_FAI_CHOICE(x) x,
#else
  #define IS_FAI_ENABLED() false
  #define IF_FAI_CHOICE(x)
#endif

#define IS_FAI_FORBIDDEN(idx) (IS_FAI_ENABLED() && isFaiForbidden(idx))

#if defined(LUA)
  #define RADIO_TOOLS
#endif

#if defined(ROTARY_ENCODER_NAVIGATION)
enum RotaryEncoderMode {
  ROTARY_ENCODER_MODE_NORMAL,
  ROTARY_ENCODER_MODE_INVERT_BOTH,
  ROTARY_ENCODER_MODE_INVERT_VERT_HORZ_NORM,
  ROTARY_ENCODER_MODE_INVERT_VERT_HORZ_ALT,
  ROTARY_ENCODER_MODE_VERT_NORM_HORZ_INVERT,
  ROTARY_ENCODER_MODE_LAST = ROTARY_ENCODER_MODE_VERT_NORM_HORZ_INVERT
};
#endif

// RESX range is used for internal calculation; The menu says -100.0 to 100.0; internally it is -1024 to 1024 to allow some optimizations
#define RESX_SHIFT 10
#define RESX       1024
#define RESXu      1024u
#define RESXul     1024ul
#define RESXl      1024l

#include "debug.h"

#include "myeeprom.h"

// TODO: move these config check macros somewhere else
#define POT_CONFIG(x) (getPotType(x))

#define IS_POT_MULTIPOS(x) (POT_CONFIG(x) == FLEX_MULTIPOS)

#define IS_POT_WITHOUT_DETENT(x) (POT_CONFIG(x) == FLEX_POT)

#define IS_SLIDER(x) (POT_CONFIG(x) == FLEX_SLIDER)

#define IS_POT_AVAILABLE(x)						\
  (POT_CONFIG(x) != FLEX_NONE && POT_CONFIG(x) < FLEX_SWITCH)

#define IS_POT_SLIDER_AVAILABLE(x) (IS_POT_AVAILABLE(x))

#define IS_MULTIPOS_CALIBRATED(cal)			\
  (cal->count > 0 && cal->count < XPOTS_MULTIPOS_COUNT)

int16_t getPotWarningPosition(uint8_t index);

#define SAVE_POT_POSITION(i) \
  g_model.potsWarnPosition[i] = getPotWarningPosition(i)

#define ANALOG_CENTER_BEEP(x) \
  (g_model.beepANACenter & ((BeepANACenter)1 << (x)))

#define PPM_CENTER                     1500

#include "fifo.h"

#if defined(CLI)
#include "cli.h"
#endif

#include "timers.h"
#include "storage/storage.h"
#include "pulses/pulses.h"
#include "pulses/modules_helpers.h"

#include "strhelpers.h"
#if defined(COLORLCD)
#include "gui_common.h"
#include "menus.h"
#include "popups.h"
#else
#include "gui.h"
#endif

#if !defined(SIMU)
  #define assert(x)
  #if !defined(DEBUG)
    #define printf printf_not_allowed
  #endif
#endif

#define THRCHK_DEADBAND                16

#if !defined(COLORLCD)
inline bool SPLASH_NEEDED()
{
  return g_eeGeneral.splashMode != 3;
}
#endif

#define SPLASH_TIMEOUT (g_eeGeneral.splashMode == -4 ? 1500 : (g_eeGeneral.splashMode <= 0 ? (400-g_eeGeneral.splashMode * 200) : (400 - g_eeGeneral.splashMode * 100)))

extern void startSplash();
extern void waitSplash();
extern void cancelSplash();

extern uint8_t heartbeat;

#include "keys.h"
#include "pwr.h"

uint16_t evalChkSum();

void alert(const char * title, const char * msg, uint8_t sound);

#if !defined(GUI)

  #define RAISE_ALERT(...)
  #define ALERT(...)

#elif defined(COLORLCD)

#define TELEMETRY_CHECK_DELAY10ms 150

bool confirmationDialog(const char *title, const char *msg, bool checkPwr = true, const std::function<bool(void)>& closeCondition = nullptr);

void raiseAlert(const char *title, const char *msg, const char *info,
                uint8_t sound);

inline void RAISE_ALERT(const char *title, const char *msg, const char *info,
                        uint8_t sound)
{
  raiseAlert(title, msg, info, sound);
}
inline void ALERT(const char *title, const char *msg, uint8_t sound)
{
  raiseAlert(title, msg, STR_PRESS_ANY_KEY_TO_SKIP, sound);
}

#else // !COLORLCD && GUI

#include "popups.h"

inline void RAISE_ALERT(const char *title, const char *msg, const char *info,
                        uint8_t sound)
{
  showAlertBox(title, msg, info, sound);
}

inline void ALERT(const char *title, const char *msg, uint8_t sound)
{
  alert(title, msg, sound);
}

#endif // !COLORLCD && GUI

extern uint32_t availableMemory();

void updateChannelOutputs();
void doMixerCalculations();
void doMixerPeriodicUpdates();

extern uint8_t currentBacklightBright;
void perMain();

getvalue_t getValue(mixsrc_t i, bool* valid = nullptr);

void flightReset(uint8_t check=true);

#define DURATION_MS_PREC2(x) ((x)/10)

void checkThrottleStick();
void checkSwitches();
void checkAlarm();
void checkAll(bool isBootCheck = false);

void getADC();

void resetBacklightTimeout();
void checkBacklight();

uint16_t isqrt32(uint32_t n);

void generalDefault();
void generalDefault(RadioData& settings);
void generalDefaultSwitches();
void generalDefaultUILanguage();

uint32_t hash(const void * ptr, uint32_t size);

inline int calcRESXto1000(int x)
{
  return divRoundClosest(x*1000, RESX);
}

inline int calcRESXto100(int x)
{
  return divRoundClosest(x*100, RESX);
}

#define g_blinkTmr10ms    (*(uint8_t*)&g_tmr10ms)

void evalAnalogControls(bool beep = true);
uint16_t anaIn(uint8_t chan);

#define FLASH_DURATION 20 /*200ms*/

USBJoystickChData * usbJChAddress(uint8_t idx);

void applyDefaultTemplate();

#define VARIO_FREQUENCY_ZERO   700/*Hz*/
#define VARIO_FREQUENCY_RANGE  1000/*Hz*/
#define VARIO_REPEAT_ZERO      500/*ms*/
#define VARIO_REPEAT_MAX       80/*ms*/

#include "telemetry/telemetry.h"
#include "crc.h"

#define PLAY_REPEAT(x)            (x)                 /* Range 0 to 15 */
#define PLAY_NOW                  0x10
#define PLAY_BACKGROUND           0x20
#define PLAY_PURE                 0x40                /* distortion-free sine */

enum AUDIO_SOUNDS {
  AUDIO_HELLO,
  AU_BYE,
  AU_THROTTLE_ALERT,
  AU_SWITCH_ALERT,
  AU_BAD_RADIODATA,
  AU_TX_BATTERY_LOW,
  AU_INACTIVITY,
  AU_RSSI_ORANGE,
  AU_RSSI_RED,
  AU_TELEMETRY_CONNECTED,
  AU_TELEMETRY_LOST,
  AU_TELEMETRY_BACK,
  AU_SENSOR_LOST,
  AU_MODEL_STILL_POWERED,
  AU_ERROR,
  AU_WARNING1,
  AU_WARNING2,
  AU_WARNING3,
  AU_STICK1_MIDDLE,
  AU_STICK2_MIDDLE,
  AU_STICK3_MIDDLE,
  AU_STICK4_MIDDLE,
  AU_POT1_MIDDLE,
  AU_POT2_MIDDLE,
#if defined(PCBX9E)
  AU_POT3_MIDDLE,
  AU_POT4_MIDDLE,
#endif //X9E
#if defined(PCBX10)
  AU_POT4_MIDDLE,
  AU_POT5_MIDDLE,
  AU_POT6_MIDDLE,
  AU_POT7_MIDDLE,
#endif //X10
  AU_SLIDER1_MIDDLE,
  AU_SLIDER2_MIDDLE,
#if defined(PCBX9E)
  AU_SLIDER3_MIDDLE,
  AU_SLIDER4_MIDDLE,
#endif // X9E
  AU_TIMER1_ELAPSED,
  AU_TIMER2_ELAPSED,
  AU_TIMER3_ELAPSED,

  AU_SPECIAL_SOUND_FIRST,
  AU_SPECIAL_SOUND_BEEP1 = AU_SPECIAL_SOUND_FIRST,
  AU_SPECIAL_SOUND_BEEP2,
  AU_SPECIAL_SOUND_BEEP3,
  AU_SPECIAL_SOUND_WARN1,
  AU_SPECIAL_SOUND_WARN2,
  AU_SPECIAL_SOUND_CHEEP,
  AU_SPECIAL_SOUND_RATATA,
  AU_SPECIAL_SOUND_TICK,
  AU_SPECIAL_SOUND_SIREN,
  AU_SPECIAL_SOUND_RING,
  AU_SPECIAL_SOUND_SCIFI,
  AU_SPECIAL_SOUND_ROBOT,
  AU_SPECIAL_SOUND_CHIRP,
  AU_SPECIAL_SOUND_TADA,
  AU_SPECIAL_SOUND_CRICKET,
  AU_SPECIAL_SOUND_ALARMC,
  AU_SPECIAL_SOUND_LAST,

  AU_NONE = 0xff
};

#if defined(AUDIO)
#include "audio.h"
#endif

#include "translations/translations.h"

#if defined(HAPTIC)
#include "haptic.h"
#endif

#include "sdcard.h"

#if defined(RTCLOCK)
#include "rtc.h"
#endif

void checkBattery();
void edgeTxClose(uint8_t shutdown=true);
void edgeTxInit();
void edgeTxResume();

constexpr uint8_t OPENTX_START_NO_SPLASH = 0x01;
constexpr uint8_t OPENTX_START_NO_CALIBRATION = 0x02;
constexpr uint8_t OPENTX_START_NO_CHECKS = 0x04;

#if STATUS_LEDS
  #define LED_ERROR_BEGIN()            ledRed()
  // Green "ready to use" if available
#if defined(LED_GREEN_GPIO) || defined(LED_STRIP_GPIO)
  #define LED_ERROR_END() ledGreen()
  #define LED_BIND() ledBlue()
#else
// Green is not available
  #define LED_ERROR_END()              ledBlue()
#endif
#else
  #define LED_ERROR_BEGIN()
  #define LED_ERROR_END()
#endif

#if LCD_W <= 212
constexpr uint8_t SD_SCREEN_FILE_LENGTH = 32;
#else
constexpr uint8_t SD_SCREEN_FILE_LENGTH = 64;
#endif

#if defined(BLUETOOTH)
#include "bluetooth.h"
#endif

constexpr uint8_t TEXT_FILENAME_MAXLEN = 40;

// Re-useable byte array to save having multiple buffers
union ReusableBuffer
{
#if !defined(COLORLCD)
  struct {
    char menu_bss[POPUP_MENU_MAX_LINES][MENU_LINE_LENGTH];
    char mainname[45]; // because reused for SD backup / restore, max backup filename 44 chars: "/MODELS/MODEL0134353-2014-06-19-04-51-27.bin"
  } modelsel;

  struct {
    char filename[TEXT_FILENAME_MAXLEN];
    char lines[NUM_BODY_LINES][LCD_COLS + 1];
    int linesCount;
    bool checklistComplete;
    bool pushMenu;
  } viewText;

  struct {
    int8_t antennaMode;
  } radioHardware;

  struct {
    uint8_t stickMode;
#if defined(ROTARY_ENCODER_NAVIGATION)
    uint8_t rotaryEncoderMode;
#endif
  } generalSettings;
#endif

  struct {
    char msg[64];
#if !defined(COLORLCD)
    int8_t antennaMode;
#endif
  } moduleSetup;

  struct {
    uint8_t state;
    union {
      struct {
        int16_t midVal;
        int16_t loVal;
        int16_t hiVal;
      } input;
      struct {
        uint8_t stepsCount;
        int16_t steps[XPOTS_MULTIPOS_COUNT];
        uint8_t lastCount;
        int16_t lastPosition;
      } xpot;
    } inputs[MAX_ANALOG_INPUTS];
  } calib;

  struct {
#if !defined(COLORLCD)
    char lines[NUM_BODY_LINES][SD_SCREEN_FILE_LENGTH+1+1]; // the last char is used to store the flags (directory) of the line
    uint16_t offset;
    uint16_t count;
    char originalName[SD_SCREEN_FILE_LENGTH+1];
#endif
  } sdManager;

#if !defined(COLORLCD)
  #define TOOL_NAME_MAX_LEN (LCD_W / FW)
  #define TOOL_PATH_MAX_LEN 40
  struct scriptInfo{
      uint8_t index;
      char label[TOOL_NAME_MAX_LEN + 1];
      uint8_t module;
      void (* tool)(event_t);
      char filename[TOOL_PATH_MAX_LEN + 1];
  };
#endif

  struct {
#if !defined(COLORLCD)
    scriptInfo script[NUM_BODY_LINES];
    uint8_t oldOffset;
    uint8_t linesCount;
#endif
  } radioTools;

};

extern ReusableBuffer reusableBuffer;

// Stick tolerance varies between transmitters, Higher is better
#define STICK_TOLERANCE 64

extern uint8_t g_vbat100mV;

inline uint8_t GET_TXBATT_BARS(uint8_t barsMax)
{
  return limit<int8_t>(0, divRoundClosest(barsMax * (g_vbat100mV - g_eeGeneral.vBatMin - 90), 30 + g_eeGeneral.vBatMax - g_eeGeneral.vBatMin), barsMax);
}

inline bool IS_TXBATT_WARNING()
{
  return g_vbat100mV <= g_eeGeneral.vBatWarn;
}

constexpr uint32_t EARTH_RADIUS = 6371009;

void varioWakeup();

#define IS_SOUND_OFF() (g_eeGeneral.beepMode == e_mode_quiet)

#define IS_IMPERIAL_ENABLE() (g_eeGeneral.imperial)

#if defined(PCBTARANIS)
  extern const unsigned char logo_taranis[];
#endif

#include "lua/lua_api.h"
#include "lua/lua_runtime.h"

enum ClipboardType {
  CLIPBOARD_TYPE_NONE,
  CLIPBOARD_TYPE_SD_FILE,
};

#if defined(SIMU)
  #define CLIPBOARD_PATH_LEN 1024
#else
  #define CLIPBOARD_PATH_LEN 32
#endif

struct Clipboard {
  ClipboardType type;
  union {
    struct {
      char directory[CLIPBOARD_PATH_LEN];
      char filename[CLIPBOARD_PATH_LEN];
    } sd;
  } data;
};

extern Clipboard clipboard;

#if defined(INTERNAL_GPS)
  #include "gps.h"
#endif

#if defined(SPACEMOUSE)
  #include "spacemouse.h"
#endif

#if defined(IMU)
#include "gyro.h"
#endif

#if defined(DEBUG_LATENCY)
extern uint8_t latencyToggleSwitch;
#endif

// Radio menu tab state
#if defined(COLORLCD)
extern bool radioThemesEnabled();
#endif
extern bool modelTelemetryEnabled();

int pwrDelayFromYaml(int delay);
int pwrDelayToYaml(int delay);
