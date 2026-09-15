/*
 * Copyright (C) EdgeTX
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

#include "radio_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dataconstants.h"
#include "debug.h"
#include "edgetx_constants.h"
#include "hal/switch_driver.h"
#include "myeeprom.h"
#include "sdcard.h"

// Bounded working-set: one input line buffer + one output line buffer,
// each fixed size, plus a handful of small boolean tracking arrays sized to
// the field/switch tables. No heap allocation, no DOM, no per-file growth.
static constexpr int RC_LINE_MAX = 128;
static constexpr int RC_VAL_MAX = 32;

enum class RcType : uint8_t { U8, I8, U16, I16, U32, BOOL, STR };

struct RadioConfigField {
  const char* key;
  RcType type;
  int32_t minVal;
  int32_t maxVal;  // for STR: maximum string length (excluding terminator)
  bool (*isAvailable)();
  int32_t (*get)(const RadioData&);
  void (*set)(RadioData&, int32_t);
  void (*getStr)(const RadioData&, char* out, size_t outSize) = nullptr;
  void (*setStr)(RadioData&, const char* val) = nullptr;
};

// --- capability predicates -------------------------------------------------
// Hardware/target facts are asked from the target/HAL (via compile-time
// capability macros here); they are never duplicated into radio.yml.

static bool rc_always_available() { return true; }

#if defined(IMU)
static bool rc_imu_available() { return true; }
#else
static bool rc_imu_available() { return false; }
#endif

#if OLED_SCREEN
static bool rc_oled_available() { return true; }
#else
static bool rc_oled_available() { return false; }
#endif

#if defined(COLORLCD)
static bool rc_colorlcd_available() { return true; }
#else
static bool rc_colorlcd_available() { return false; }
#endif

#if defined(COLORLCD) && defined(USB_CHARGE_CONTROL)
static bool rc_usbcharge_available() { return true; }
#else
static bool rc_usbcharge_available() { return false; }
#endif

#if !defined(COLORLCD) && LCD_W == 128
static bool rc_invertlcd_available() { return true; }
#else
static bool rc_invertlcd_available() { return false; }
#endif

#if !defined(PCBHORUS)
static bool rc_currmodel_available() { return true; }
#else
static bool rc_currmodel_available() { return false; }
#endif

#if defined(USE_HATS_AS_KEYS)
static bool rc_hats_available() { return true; }
#else
static bool rc_hats_available() { return false; }
#endif

#if defined(STM32F4)
static bool rc_uartsamplemode_available() { return true; }
#else
static bool rc_uartsamplemode_available() { return false; }
#endif

#if defined(STICK_DEAD_ZONE)
static bool rc_stickdeadzone_available() { return true; }
#else
static bool rc_stickdeadzone_available() { return false; }
#endif

// --- field accessors ---------------------------------------------------

// Generates a plain get/set pair for a straightforward (non-array,
// non-custom-encoded) RadioData member. Bitfield members are handled the
// same way as plain members -- ordinary member access, no offsets/bit
// masks needed.
#define RC_SIMPLE(name, ctype)                                            \
  static int32_t g_##name(const RadioData& d) { return (int32_t)d.name; } \
  static void s_##name(RadioData& d, int32_t v) { d.name = (ctype)v; }

RC_SIMPLE(timezoneMinutes, int8_t)
RC_SIMPLE(ppmunit, uint8_t)
RC_SIMPLE(antennaMode, int8_t)
RC_SIMPLE(disableRtcWarning, uint8_t)
RC_SIMPLE(keysBacklight, uint8_t)
RC_SIMPLE(dontPlayHello, uint8_t)
RC_SIMPLE(internalModule, uint8_t)
RC_SIMPLE(view, uint8_t)
RC_SIMPLE(fai, uint8_t)
RC_SIMPLE(beepMode, int8_t)
RC_SIMPLE(alarmsFlash, uint8_t)
RC_SIMPLE(disableMemoryWarning, uint8_t)
RC_SIMPLE(disableAlarmWarning, uint8_t)
RC_SIMPLE(stickMode, uint8_t)
RC_SIMPLE(timezone, int8_t)
RC_SIMPLE(internalModuleBaudrate, uint8_t)
RC_SIMPLE(splashMode, int8_t)
RC_SIMPLE(switchesDelay, int8_t)
RC_SIMPLE(lightAutoOff, uint8_t)
RC_SIMPLE(templateSetup, uint8_t)
RC_SIMPLE(hapticLength, int8_t)
RC_SIMPLE(beepLength, int8_t)
RC_SIMPLE(hapticStrength, uint8_t)
RC_SIMPLE(gpsFormat, uint8_t)
RC_SIMPLE(audioMuteEnable, uint8_t)
RC_SIMPLE(speakerPitch, uint8_t)
RC_SIMPLE(vBatMin, int8_t)
RC_SIMPLE(vBatMax, int8_t)
RC_SIMPLE(bluetoothBaudrate, uint8_t)
RC_SIMPLE(bluetoothMode, uint8_t)
RC_SIMPLE(countryCode, uint8_t)
RC_SIMPLE(pwrOnSpeed, int8_t)
RC_SIMPLE(pwrOffSpeed, int8_t)
RC_SIMPLE(noJitterFilter, uint8_t)
RC_SIMPLE(disableRssiPoweroffAlarm, uint8_t)
RC_SIMPLE(USBMode, uint8_t)
RC_SIMPLE(beepVolume, int8_t)
RC_SIMPLE(wavVolume, int8_t)
RC_SIMPLE(varioVolume, int8_t)
RC_SIMPLE(backgroundVolume, int8_t)
RC_SIMPLE(varioPitch, int8_t)
RC_SIMPLE(varioRange, int8_t)
RC_SIMPLE(varioRepeat, int8_t)
RC_SIMPLE(rotEncMode, uint8_t)
RC_SIMPLE(backlightSrc, int16_t)
RC_SIMPLE(modelCurvesDisabled, uint8_t)
RC_SIMPLE(volumeSrc, int16_t)
RC_SIMPLE(modelCustomScriptsDisabled, uint8_t)
RC_SIMPLE(modelTelemetryDisabled, uint8_t)
RC_SIMPLE(modelQuickSelect, uint8_t)
RC_SIMPLE(oneLogPerDay, uint8_t)
RC_SIMPLE(keyLockEnabled, uint8_t)
RC_SIMPLE(pwrOffIfInactive, uint8_t)

static int32_t g_globalTimer(const RadioData& d)
{
  return (int32_t)d.globalTimer;
}
static void s_globalTimer(RadioData& d, int32_t v)
{
  d.globalTimer = (uint32_t)v;
}

#if !defined(PCBHORUS)
RC_SIMPLE(currModel, int8_t)
#endif

#if defined(USE_HATS_AS_KEYS)
RC_SIMPLE(hatsMode, uint8_t)
#endif

#if defined(STM32F4)
RC_SIMPLE(uartSampleMode, int8_t)
#endif

#if defined(STICK_DEAD_ZONE)
RC_SIMPLE(stickDeadZone, uint8_t)
#endif

#if defined(COLORLCD)
RC_SIMPLE(labelSingleSelect, uint8_t)
RC_SIMPLE(labelMultiMode, uint8_t)
RC_SIMPLE(favMultiMode, uint8_t)
RC_SIMPLE(modelSelectLayout, uint8_t)
#if defined(USB_CHARGE_CONTROL)
RC_SIMPLE(usbChargeDisabled, uint8_t)
#endif
#endif

#if !defined(COLORLCD) && LCD_W == 128
RC_SIMPLE(invertLCD, uint8_t)
#endif

// Fixed-width (non-null-terminated) 2-letter language codes.
static void g_uiLanguage(const RadioData& d, char* out, size_t outSize)
{
  size_t n = 0;
  for (; n < 2 && n < outSize - 1; n++)
    out[n] = d.uiLanguage[n] ? d.uiLanguage[n] : ' ';
  out[n] = 0;
}
static void s_uiLanguage(RadioData& d, const char* val)
{
  size_t len = strlen(val);
  for (size_t i = 0; i < 2; i++) d.uiLanguage[i] = (i < len) ? val[i] : ' ';
}

static void g_ttsLanguage(const RadioData& d, char* out, size_t outSize)
{
  size_t n = 0;
  for (; n < 2 && n < outSize - 1; n++)
    out[n] = d.ttsLanguage[n] ? d.ttsLanguage[n] : ' ';
  out[n] = 0;
}
static void s_ttsLanguage(RadioData& d, const char* val)
{
  size_t len = strlen(val);
  for (size_t i = 0; i < 2; i++) d.ttsLanguage[i] = (i < len) ? val[i] : ' ';
}

#if defined(COLORLCD)
static void g_selectedTheme(const RadioData& d, char* out, size_t outSize)
{
  snprintf(out, outSize, "%s", d.selectedTheme);
}
static void s_selectedTheme(RadioData& d, const char* val)
{
  strncpy(d.selectedTheme, val, sizeof(d.selectedTheme) - 1);
  d.selectedTheme[sizeof(d.selectedTheme) - 1] = 0;
}
#endif

static int32_t g_vBatWarn(const RadioData& d) { return d.vBatWarn; }
static void s_vBatWarn(RadioData& d, int32_t v) { d.vBatWarn = (uint8_t)v; }

static int32_t g_txVoltageCalibration(const RadioData& d)
{
  return d.txVoltageCalibration;
}
static void s_txVoltageCalibration(RadioData& d, int32_t v)
{
  d.txVoltageCalibration = (int8_t)v;
}

static int32_t g_backlightMode(const RadioData& d) { return d.backlightMode; }
static void s_backlightMode(RadioData& d, int32_t v)
{
  d.backlightMode = (uint8_t)v;
}

static int32_t g_backlightBright(const RadioData& d)
{
  return d.backlightBright;
}
static void s_backlightBright(RadioData& d, int32_t v)
{
  d.backlightBright = (uint8_t)v;
}

static int32_t g_inactivityTimer(const RadioData& d)
{
  return d.inactivityTimer;
}
static void s_inactivityTimer(RadioData& d, int32_t v)
{
  d.inactivityTimer = (uint8_t)v;
}

static int32_t g_speakerVolume(const RadioData& d) { return d.speakerVolume; }
static void s_speakerVolume(RadioData& d, int32_t v)
{
  d.speakerVolume = (int8_t)v;
}

static int32_t g_hapticMode(const RadioData& d) { return d.hapticMode; }
static void s_hapticMode(RadioData& d, int32_t v) { d.hapticMode = (int8_t)v; }

static int32_t g_disablePwrOnOffHaptic(const RadioData& d)
{
  return d.disablePwrOnOffHaptic;
}
static void s_disablePwrOnOffHaptic(RadioData& d, int32_t v)
{
  d.disablePwrOnOffHaptic = v ? 1 : 0;
}

static int32_t g_imperial(const RadioData& d) { return d.imperial; }
static void s_imperial(RadioData& d, int32_t v) { d.imperial = v ? 1 : 0; }

static int32_t g_adjustRTC(const RadioData& d) { return d.adjustRTC; }
static void s_adjustRTC(RadioData& d, int32_t v) { d.adjustRTC = v ? 1 : 0; }

#if defined(IMU)
static int32_t g_imuMax(const RadioData& d) { return d.imuMax; }
static void s_imuMax(RadioData& d, int32_t v) { d.imuMax = (int8_t)v; }
static int32_t g_imuOffset(const RadioData& d) { return d.imuOffset; }
static void s_imuOffset(RadioData& d, int32_t v) { d.imuOffset = (int8_t)v; }
static int32_t g_imuInvert(const RadioData& d) { return d.imuInvert; }
static void s_imuInvert(RadioData& d, int32_t v) { d.imuInvert = (uint8_t)v; }
#endif

#if OLED_SCREEN
static int32_t g_contrast(const RadioData& d) { return d.contrast; }
static void s_contrast(RadioData& d, int32_t v) { d.contrast = (uint8_t)v; }
#endif

#if defined(COLORLCD)
static int32_t g_radioThemesDisabled(const RadioData& d)
{
  return d.radioThemesDisabled;
}
static void s_radioThemesDisabled(RadioData& d, int32_t v)
{
  d.radioThemesDisabled = v ? 1 : 0;
}
#endif

// Universal schema: every retained target shares this table. Fields that a
// given target's hardware does not support are still "known" (present in
// this table); they are simply reported unavailable via isAvailable().
//
// NOTE: this is a representative subset of radio settings (the core
// streaming engine). Remaining fields, and nested sub-trees such as
// per-switch configuration, are currently treated as unknown data and
// therefore preserved unchanged across saves -- see README/report for the
// migration plan.
static const RadioConfigField rc_fields[] = {
    {"vBatWarn", RcType::U8, 0, 255, rc_always_available, g_vBatWarn,
     s_vBatWarn},
    {"txVoltageCalibration", RcType::I8, -50, 50, rc_always_available,
     g_txVoltageCalibration, s_txVoltageCalibration},
    {"backlightMode", RcType::U8, 0, 7, rc_always_available, g_backlightMode,
     s_backlightMode},
    {"backlightBright", RcType::U8, 0, 100, rc_always_available,
     g_backlightBright, s_backlightBright},
    {"inactivityTimer", RcType::U8, 0, 255, rc_always_available,
     g_inactivityTimer, s_inactivityTimer},
    {"speakerVolume", RcType::I8, -125, 125, rc_always_available,
     g_speakerVolume, s_speakerVolume},
    {"hapticMode", RcType::I8, -2, 2, rc_always_available, g_hapticMode,
     s_hapticMode},
    {"disablePwrOnOffHaptic", RcType::BOOL, 0, 1, rc_always_available,
     g_disablePwrOnOffHaptic, s_disablePwrOnOffHaptic},
    {"imperial", RcType::BOOL, 0, 1, rc_always_available, g_imperial,
     s_imperial},
    {"adjustRTC", RcType::BOOL, 0, 1, rc_always_available, g_adjustRTC,
     s_adjustRTC},
#if defined(IMU)
    {"imuMax", RcType::I8, -125, 125, rc_imu_available, g_imuMax, s_imuMax},
    {"imuOffset", RcType::I8, -125, 125, rc_imu_available, g_imuOffset,
     s_imuOffset},
    {"imuInvert", RcType::U8, 0, 3, rc_imu_available, g_imuInvert, s_imuInvert},
#else
    // No IMU hardware on this target: the fields are still recognised
    // ("known"), just never applied. get/set are unused (isAvailable() is
    // always false), so it is safe that the struct has no such members here.
    {"imuMax", RcType::I8, -125, 125, rc_imu_available, nullptr, nullptr},
    {"imuOffset", RcType::I8, -125, 125, rc_imu_available, nullptr, nullptr},
    {"imuInvert", RcType::U8, 0, 3, rc_imu_available, nullptr, nullptr},
#endif
#if OLED_SCREEN
    {"contrast", RcType::U8, 0, 63, rc_oled_available, g_contrast, s_contrast},
#else
    // Colour/backlit targets use backlightBright instead of contrast; the
    // field is still known, just unavailable on this hardware.
    {"contrast", RcType::U8, 0, 63, rc_oled_available, nullptr, nullptr},
#endif
#if defined(COLORLCD)
    {"radioThemesDisabled", RcType::BOOL, 0, 1, rc_colorlcd_available,
     g_radioThemesDisabled, s_radioThemesDisabled},
#else
    {"radioThemesDisabled", RcType::BOOL, 0, 1, rc_colorlcd_available, nullptr,
     nullptr},
#endif

    {"timezoneMinutes", RcType::I8, -3, 3, rc_always_available,
     g_timezoneMinutes, s_timezoneMinutes},
    {"ppmunit", RcType::U8, 0, 3, rc_always_available, g_ppmunit, s_ppmunit},
    {"antennaMode", RcType::I8, -2, 1, rc_always_available, g_antennaMode,
     s_antennaMode},
    {"disableRtcWarning", RcType::BOOL, 0, 1, rc_always_available,
     g_disableRtcWarning, s_disableRtcWarning},
    {"keysBacklight", RcType::BOOL, 0, 1, rc_always_available, g_keysBacklight,
     s_keysBacklight},
    {"dontPlayHello", RcType::BOOL, 0, 1, rc_always_available, g_dontPlayHello,
     s_dontPlayHello},
    {"internalModule", RcType::U8, 0, 255, rc_always_available,
     g_internalModule, s_internalModule},
    {"view", RcType::U8, 0, 255, rc_always_available, g_view, s_view},
    {"fai", RcType::BOOL, 0, 1, rc_always_available, g_fai, s_fai},
    {"beepMode", RcType::I8, -2, 1, rc_always_available, g_beepMode,
     s_beepMode},
    {"alarmsFlash", RcType::BOOL, 0, 1, rc_always_available, g_alarmsFlash,
     s_alarmsFlash},
    {"disableMemoryWarning", RcType::BOOL, 0, 1, rc_always_available,
     g_disableMemoryWarning, s_disableMemoryWarning},
    {"disableAlarmWarning", RcType::BOOL, 0, 1, rc_always_available,
     g_disableAlarmWarning, s_disableAlarmWarning},
    {"stickMode", RcType::U8, 0, 3, rc_always_available, g_stickMode,
     s_stickMode},
    {"timezone", RcType::I8, -16, 15, rc_always_available, g_timezone,
     s_timezone},
    {"internalModuleBaudrate", RcType::U8, 0, 7, rc_always_available,
     g_internalModuleBaudrate, s_internalModuleBaudrate},
    {"splashMode", RcType::I8, -4, 3, rc_always_available, g_splashMode,
     s_splashMode},
    {"switchesDelay", RcType::I8, -100, 100, rc_always_available,
     g_switchesDelay, s_switchesDelay},
    {"lightAutoOff", RcType::U8, 0, 255, rc_always_available, g_lightAutoOff,
     s_lightAutoOff},
    {"templateSetup", RcType::U8, 0, 255, rc_always_available, g_templateSetup,
     s_templateSetup},
    {"hapticLength", RcType::I8, -100, 100, rc_always_available, g_hapticLength,
     s_hapticLength},
    {"beepLength", RcType::I8, -4, 3, rc_always_available, g_beepLength,
     s_beepLength},
    {"hapticStrength", RcType::U8, 0, 7, rc_always_available, g_hapticStrength,
     s_hapticStrength},
    {"gpsFormat", RcType::BOOL, 0, 1, rc_always_available, g_gpsFormat,
     s_gpsFormat},
    {"audioMuteEnable", RcType::BOOL, 0, 1, rc_always_available,
     g_audioMuteEnable, s_audioMuteEnable},
    {"speakerPitch", RcType::U8, 0, 255, rc_always_available, g_speakerPitch,
     s_speakerPitch},
    {"vBatMin", RcType::I8, -128, 127, rc_always_available, g_vBatMin,
     s_vBatMin},
    {"vBatMax", RcType::I8, -128, 127, rc_always_available, g_vBatMax,
     s_vBatMax},
    {"globalTimer", RcType::U32, 0, 0, rc_always_available, g_globalTimer,
     s_globalTimer},
    {"bluetoothBaudrate", RcType::U8, 0, 15, rc_always_available,
     g_bluetoothBaudrate, s_bluetoothBaudrate},
    {"bluetoothMode", RcType::U8, 0, 15, rc_always_available, g_bluetoothMode,
     s_bluetoothMode},
    {"countryCode", RcType::U8, 0, 3, rc_always_available, g_countryCode,
     s_countryCode},
    {"pwrOnSpeed", RcType::I8, -4, 3, rc_always_available, g_pwrOnSpeed,
     s_pwrOnSpeed},
    {"pwrOffSpeed", RcType::I8, -4, 3, rc_always_available, g_pwrOffSpeed,
     s_pwrOffSpeed},
    {"noJitterFilter", RcType::BOOL, 0, 1, rc_always_available,
     g_noJitterFilter, s_noJitterFilter},
    {"disableRssiPoweroffAlarm", RcType::BOOL, 0, 1, rc_always_available,
     g_disableRssiPoweroffAlarm, s_disableRssiPoweroffAlarm},
    {"USBMode", RcType::U8, 0, 3, rc_always_available, g_USBMode, s_USBMode},
    {"beepVolume", RcType::I8, -8, 7, rc_always_available, g_beepVolume,
     s_beepVolume},
    {"wavVolume", RcType::I8, -8, 7, rc_always_available, g_wavVolume,
     s_wavVolume},
    {"varioVolume", RcType::I8, -8, 7, rc_always_available, g_varioVolume,
     s_varioVolume},
    {"backgroundVolume", RcType::I8, -8, 7, rc_always_available,
     g_backgroundVolume, s_backgroundVolume},
    {"varioPitch", RcType::I8, -128, 127, rc_always_available, g_varioPitch,
     s_varioPitch},
    {"varioRange", RcType::I8, -128, 127, rc_always_available, g_varioRange,
     s_varioRange},
    {"varioRepeat", RcType::I8, -128, 127, rc_always_available, g_varioRepeat,
     s_varioRepeat},
    {"rotEncMode", RcType::U8, 0, 7, rc_always_available, g_rotEncMode,
     s_rotEncMode},
    {"backlightSrc", RcType::I16, -512, 511, rc_always_available,
     g_backlightSrc, s_backlightSrc},
    {"modelCurvesDisabled", RcType::BOOL, 0, 1, rc_always_available,
     g_modelCurvesDisabled, s_modelCurvesDisabled},
    {"volumeSrc", RcType::I16, -512, 511, rc_always_available, g_volumeSrc,
     s_volumeSrc},
    {"modelCustomScriptsDisabled", RcType::BOOL, 0, 1, rc_always_available,
     g_modelCustomScriptsDisabled, s_modelCustomScriptsDisabled},
    {"modelTelemetryDisabled", RcType::BOOL, 0, 1, rc_always_available,
     g_modelTelemetryDisabled, s_modelTelemetryDisabled},
    {"modelQuickSelect", RcType::BOOL, 0, 1, rc_always_available,
     g_modelQuickSelect, s_modelQuickSelect},
    {"oneLogPerDay", RcType::BOOL, 0, 1, rc_always_available, g_oneLogPerDay,
     s_oneLogPerDay},
    {"keyLockEnabled", RcType::BOOL, 0, 1, rc_always_available,
     g_keyLockEnabled, s_keyLockEnabled},
    {"pwrOffIfInactive", RcType::U8, 0, 255, rc_always_available,
     g_pwrOffIfInactive, s_pwrOffIfInactive},

#if !defined(PCBHORUS)
    {"currModel", RcType::I8, -128, 127, rc_currmodel_available, g_currModel,
     s_currModel},
#else
    {"currModel", RcType::I8, -128, 127, rc_currmodel_available, nullptr,
     nullptr},
#endif
#if defined(USE_HATS_AS_KEYS)
    {"hatsMode", RcType::U8, 0, 3, rc_hats_available, g_hatsMode, s_hatsMode},
#else
    {"hatsMode", RcType::U8, 0, 3, rc_hats_available, nullptr, nullptr},
#endif
#if defined(STM32F4)
    {"uartSampleMode", RcType::I8, -2, 1, rc_uartsamplemode_available,
     g_uartSampleMode, s_uartSampleMode},
#else
    {"uartSampleMode", RcType::I8, -2, 1, rc_uartsamplemode_available, nullptr,
     nullptr},
#endif
#if defined(STICK_DEAD_ZONE)
    {"stickDeadZone", RcType::U8, 0, 7, rc_stickdeadzone_available,
     g_stickDeadZone, s_stickDeadZone},
#else
    {"stickDeadZone", RcType::U8, 0, 7, rc_stickdeadzone_available, nullptr,
     nullptr},
#endif
#if defined(COLORLCD)
    {"labelSingleSelect", RcType::BOOL, 0, 1, rc_colorlcd_available,
     g_labelSingleSelect, s_labelSingleSelect},
    {"labelMultiMode", RcType::BOOL, 0, 1, rc_colorlcd_available,
     g_labelMultiMode, s_labelMultiMode},
    {"favMultiMode", RcType::BOOL, 0, 1, rc_colorlcd_available, g_favMultiMode,
     s_favMultiMode},
    {"modelSelectLayout", RcType::U8, 0, 3, rc_colorlcd_available,
     g_modelSelectLayout, s_modelSelectLayout},
#else
    {"labelSingleSelect", RcType::BOOL, 0, 1, rc_colorlcd_available, nullptr,
     nullptr},
    {"labelMultiMode", RcType::BOOL, 0, 1, rc_colorlcd_available, nullptr,
     nullptr},
    {"favMultiMode", RcType::BOOL, 0, 1, rc_colorlcd_available, nullptr,
     nullptr},
    {"modelSelectLayout", RcType::U8, 0, 3, rc_colorlcd_available, nullptr,
     nullptr},
#endif
#if defined(COLORLCD) && defined(USB_CHARGE_CONTROL)
    {"usbChargeDisabled", RcType::BOOL, 0, 1, rc_usbcharge_available,
     g_usbChargeDisabled, s_usbChargeDisabled},
#else
    {"usbChargeDisabled", RcType::BOOL, 0, 1, rc_usbcharge_available, nullptr,
     nullptr},
#endif
#if !defined(COLORLCD) && LCD_W == 128
    {"invertLCD", RcType::BOOL, 0, 1, rc_invertlcd_available, g_invertLCD,
     s_invertLCD},
#else
    {"invertLCD", RcType::BOOL, 0, 1, rc_invertlcd_available, nullptr, nullptr},
#endif

    {"uiLanguage", RcType::STR, 0, 2, rc_always_available, nullptr, nullptr,
     g_uiLanguage, s_uiLanguage},
    {"ttsLanguage", RcType::STR, 0, 2, rc_always_available, nullptr, nullptr,
     g_ttsLanguage, s_ttsLanguage},
#if defined(COLORLCD)
    {"selectedTheme", RcType::STR, 0, SELECTED_THEME_NAME_LEN - 1,
     rc_colorlcd_available, nullptr, nullptr, g_selectedTheme, s_selectedTheme},
#else
    {"selectedTheme", RcType::STR, 0, 31, rc_colorlcd_available, nullptr,
     nullptr, nullptr, nullptr},
#endif
};

static constexpr size_t rc_field_count =
    sizeof(rc_fields) / sizeof(rc_fields[0]);

// --- small line helpers (no heap, fixed buffers) ----------------------

static size_t rc_indent(const char* line)
{
  size_t n = 0;
  while (line[n] == ' ') n++;
  return n;
}

// Splits "key: value" at any indentation level. Returns false if the line
// is not a "key:[ value]" pair (e.g. a list item or blank line).
static bool rc_split_any(const char* line, size_t* indent, const char** key,
                         size_t* keyLen, const char** val)
{
  size_t ind = rc_indent(line);
  const char* p = line + ind;
  if (*p == '-' || *p == 0) return false;

  const char* colon = strchr(p, ':');
  if (!colon || colon == p) return false;

  *indent = ind;
  *key = p;
  *keyLen = (size_t)(colon - p);

  const char* v = colon + 1;
  while (*v == ' ') v++;
  *val = v;
  return true;
}

static void rc_strip_eol(char* line)
{
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
    line[--len] = 0;
  }
}

static int rc_find_field(const char* key, size_t keyLen)
{
  for (size_t i = 0; i < rc_field_count; i++) {
    if (strlen(rc_fields[i].key) == keyLen &&
        strncmp(rc_fields[i].key, key, keyLen) == 0) {
      return (int)i;
    }
  }
  return -1;
}

static bool rc_parse_value(const RadioConfigField& f, const char* val,
                           int32_t* out)
{
  if (f.type == RcType::BOOL) {
    if (!strcmp(val, "true") || !strcmp(val, "1")) {
      *out = 1;
      return true;
    }
    if (!strcmp(val, "false") || !strcmp(val, "0")) {
      *out = 0;
      return true;
    }
    return false;
  }

  if (f.type == RcType::U32) {
    char* end = nullptr;
    unsigned long uv = strtoul(val, &end, 10);
    if (end == val) return false;
    *out = (int32_t)(uint32_t)uv;
    return true;
  }

  char* end = nullptr;
  long v = strtol(val, &end, 10);
  if (end == val) return false;  // not a number
  if (v < f.minVal || v > f.maxVal) return false;
  *out = (int32_t)v;
  return true;
}

static void rc_format_value(const RadioConfigField& f, int32_t v, char* out,
                            size_t outSize)
{
  if (f.type == RcType::BOOL) {
    snprintf(out, outSize, "%s", v ? "true" : "false");
  } else if (f.type == RcType::U32) {
    snprintf(out, outSize, "%lu", (unsigned long)(uint32_t)v);
  } else {
    snprintf(out, outSize, "%ld", (long)v);
  }
}

// --- switches: nested per-switch known/unavailable sub-tree ------------
// Demonstrates a nested known field whose *elements* are individually
// capability-dependent (switch index availability comes from the HAL, not
// from radio.yml). Persistent key names ("SA".."ST") are stable schema
// identifiers, independent of any user-assigned custom switch name.

static constexpr int RC_MAX_SWITCHES = MAX_SWITCHES;

static const char* const rc_switch_keys[RC_MAX_SWITCHES] = {
    "SA", "SB", "SC", "SD", "SE", "SF", "SG", "SH", "SI", "SJ",
    "SK", "SL", "SM", "SN", "SO", "SP", "SQ", "SR", "SS", "ST",
};

static bool rc_switch_available(int idx)
{
  return idx >= 0 && idx < switchGetMaxAllSwitches();
}

static int rc_switch_index(const char* key, size_t keyLen)
{
  for (int i = 0; i < RC_MAX_SWITCHES; i++) {
    if (strlen(rc_switch_keys[i]) == keyLen &&
        strncmp(rc_switch_keys[i], key, keyLen) == 0) {
      return i;
    }
  }
  return -1;
}

static void rc_switch_type_to_str(SwitchConfig c, char* out, size_t outSize)
{
  const char* s = "none";
  switch (c) {
    case SWITCH_TOGGLE:
      s = "toggle";
      break;
    case SWITCH_2POS:
      s = "2pos";
      break;
    case SWITCH_3POS:
      s = "3pos";
      break;
    default:
      s = "none";
      break;
  }
  snprintf(out, outSize, "%s", s);
}

static bool rc_switch_str_to_type(const char* val, SwitchConfig* out)
{
  if (!strcmp(val, "toggle")) {
    *out = SWITCH_TOGGLE;
    return true;
  }
  if (!strcmp(val, "2pos")) {
    *out = SWITCH_2POS;
    return true;
  }
  if (!strcmp(val, "3pos")) {
    *out = SWITCH_3POS;
    return true;
  }
  if (!strcmp(val, "none")) {
    *out = SWITCH_NONE;
    return true;
  }
  return false;
}

// --- tiny buffered line reader/writer ---------------------------------
// FF_USE_STRFUNC is disabled in this project's FatFs configuration, so
// f_gets()/f_puts() are unavailable. These wrappers provide the same
// bounded-memory line semantics on top of the always-available f_read()/
// f_write(), using one small fixed-size refill buffer.

static constexpr int RC_IO_BUF = 64;

struct RcLineReader {
  FIL* file;
  char buf[RC_IO_BUF];
  UINT len = 0;
  UINT pos = 0;
  bool eof = false;
  char unreadBuf[RC_LINE_MAX] = {};
  bool hasUnread = false;

  explicit RcLineReader(FIL* f) : file(f) {}

  // Pushes a single already-consumed line back, so the next call to next()
  // returns it again. Used to end nested-block parsing on a look-ahead
  // line that belongs to the caller's outer loop.
  void unread(const char* line)
  {
    snprintf(unreadBuf, sizeof(unreadBuf), "%s", line);
    hasUnread = true;
  }

  // Reads the next line (without trailing '\n'/'\r') into `out` (bounded
  // to outSize-1 chars; any remainder of an over-long line is discarded).
  // Returns false once there is nothing left to read.
  bool next(char* out, size_t outSize)
  {
    if (hasUnread) {
      snprintf(out, outSize, "%s", unreadBuf);
      hasUnread = false;
      return true;
    }

    if (eof && pos >= len) return false;

    size_t n = 0;
    bool gotAny = false;
    for (;;) {
      if (pos >= len) {
        if (eof) break;
        FRESULT r = f_read(file, buf, sizeof(buf), &len);
        pos = 0;
        if (r != FR_OK || len == 0) {
          eof = true;
          if (len == 0) break;
        }
      }
      if (pos >= len) break;
      gotAny = true;
      char c = buf[pos++];
      if (c == '\n') break;
      if (n < outSize - 1) out[n++] = c;
    }
    out[n] = 0;
    return gotAny;
  }
};

static bool rc_write(FIL* file, const char* str)
{
  UINT written;
  size_t len = strlen(str);
  FRESULT r = f_write(file, str, (UINT)len, &written);
  return r == FR_OK && written == len;
}

// ---------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------

RadioConfigLoadResult radioConfigLoad(const char* path)
{
  RadioConfigLoadResult result;

  bool seen[rc_field_count] = {};
  bool switchSeen[RC_MAX_SWITCHES] = {};

  FIL file;
  if (f_open(&file, path, FA_OPEN_EXISTING | FA_READ) != FR_OK) {
    result.fileMissing = true;
  } else {
    char line[RC_LINE_MAX];
    RcLineReader reader(&file);
    while (reader.next(line, sizeof(line))) {
      rc_strip_eol(line);

      size_t indent;
      const char* key;
      size_t keyLen;
      const char* val;
      if (!rc_split_any(line, &indent, &key, &keyLen, &val)) continue;
      if (indent != 0) continue;  // stray indented line owned by nothing

      if (keyLen == 7 && strncmp(key, "version", 7) == 0) continue;

      if (keyLen == 8 && strncmp(key, "switches", 8) == 0 && *val == 0) {
        // Nested known sub-tree: each switch letter is individually
        // known+available/unavailable depending on hardware switch count.
        int curIdx = -1;
        size_t switchIndent = 0;
        bool haveSwitchIndent = false;
        char nested[RC_LINE_MAX];
        while (reader.next(nested, sizeof(nested))) {
          rc_strip_eol(nested);
          size_t nIndent;
          const char* nKey;
          size_t nKeyLen;
          const char* nVal;
          if (!rc_split_any(nested, &nIndent, &nKey, &nKeyLen, &nVal) ||
              nIndent == 0) {
            reader.unread(nested);
            break;
          }
          if (!haveSwitchIndent) {
            switchIndent = nIndent;
            haveSwitchIndent = true;
          }
          if (nIndent == switchIndent && *nVal == 0) {
            curIdx = rc_switch_index(nKey, nKeyLen);
            if (curIdx >= 0) switchSeen[curIdx] = true;
          } else if (nIndent > switchIndent && curIdx >= 0 && nKeyLen == 4 &&
                     strncmp(nKey, "type", 4) == 0) {
            if (rc_switch_available(curIdx)) {
              SwitchConfig sc;
              if (rc_switch_str_to_type(nVal, &sc)) {
                g_eeGeneral.switchSetType(curIdx, sc);
              } else {
                result.invalidFields++;
              }
            }
            // known+unavailable -- recognised, never applied
          }
          // any other per-switch attribute (e.g. a custom "name") is
          // unknown to this pass; ignored at runtime, preserved on save.
        }
        continue;
      }

      int idx = rc_find_field(key, keyLen);
      if (idx < 0)
        continue;  // unknown -- ignored at runtime, preserved on save

      seen[idx] = true;
      const RadioConfigField& f = rc_fields[idx];
      if (!f.isAvailable()) continue;  // known+unavailable -- never applied

      if (f.type == RcType::STR) {
        if (strlen(val) > (size_t)f.maxVal) {
          TRACE("radio_config: '%s' value too long - keeping default", f.key);
          result.invalidFields++;
          continue;
        }
        f.setStr(g_eeGeneral, val);
        continue;
      }

      int32_t v;
      if (rc_parse_value(f, val, &v)) {
        f.set(g_eeGeneral, v);
      } else {
        TRACE("radio_config: invalid value for '%s' - keeping default", f.key);
        result.invalidFields++;
      }
    }
    f_close(&file);
  }

  for (size_t i = 0; i < rc_field_count; i++) {
    if (rc_fields[i].isAvailable() && !seen[i]) {
      result.missingFields++;
    }
  }
  for (int i = 0; i < RC_MAX_SWITCHES; i++) {
    if (rc_switch_available(i) && !switchSeen[i]) {
      result.missingFields++;
    }
  }

  result.dirty = result.fileMissing || result.missingFields > 0;
  return result;
}

// ---------------------------------------------------------------------
// Save (bounded-memory streaming merge)
// ---------------------------------------------------------------------

const char* radioConfigSave(const char* path, const char* tmpPath)
{
  FIL out;
  if (f_open(&out, tmpPath, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
    return SDCARD_ERROR(FR_DISK_ERR);
  }

  bool ok = true;
  char lineOut[RC_LINE_MAX];

  snprintf(lineOut, sizeof(lineOut), "version: %lu\n",
           (unsigned long)RADIO_CONFIG_SCHEMA_VERSION);
  if (!rc_write(&out, lineOut)) ok = false;

  bool emitted[rc_field_count] = {};
  bool switchEmitted[RC_MAX_SWITCHES] = {};
  bool switchesBlockHandled = false;

  FIL in;
  bool haveSource =
      ok && (f_open(&in, path, FA_OPEN_EXISTING | FA_READ) == FR_OK);
  if (haveSource) {
    char line[RC_LINE_MAX];
    RcLineReader reader(&in);
    while (ok && reader.next(line, sizeof(line))) {
      // `line` already has its EOL stripped by the reader; re-emit our own
      // EOL when preserving unknown/unavailable content verbatim.
      size_t indent;
      const char* key;
      size_t keyLen;
      const char* val;
      if (!rc_split_any(line, &indent, &key, &keyLen, &val) || indent != 0) {
        if (!rc_write(&out, line) || !rc_write(&out, "\n")) ok = false;
        continue;
      }

      if (keyLen == 7 && strncmp(key, "version", 7) == 0) {
        continue;  // we already wrote our own version line
      }

      if (keyLen == 8 && strncmp(key, "switches", 8) == 0 && *val == 0) {
        switchesBlockHandled = true;
        if (!rc_write(&out, "switches:\n")) ok = false;

        int curIdx = -1;
        size_t switchIndent = 0;
        bool haveSwitchIndent = false;
        char nested[RC_LINE_MAX];
        while (ok && reader.next(nested, sizeof(nested))) {
          rc_strip_eol(nested);
          size_t nIndent;
          const char* nKey;
          size_t nKeyLen;
          const char* nVal;
          if (!rc_split_any(nested, &nIndent, &nKey, &nKeyLen, &nVal) ||
              nIndent == 0) {
            reader.unread(nested);
            break;
          }
          if (!haveSwitchIndent) {
            switchIndent = nIndent;
            haveSwitchIndent = true;
          }

          if (nIndent == switchIndent && *nVal == 0) {
            curIdx = rc_switch_index(nKey, nKeyLen);
            if (curIdx >= 0) switchEmitted[curIdx] = true;
            if (!rc_write(&out, nested) || !rc_write(&out, "\n")) ok = false;
          } else if (nIndent > switchIndent && curIdx >= 0 && nKeyLen == 4 &&
                     strncmp(nKey, "type", 4) == 0 &&
                     rc_switch_available(curIdx)) {
            char valStr[RC_VAL_MAX];
            rc_switch_type_to_str(g_eeGeneral.switchType(curIdx), valStr,
                                  sizeof(valStr));
            snprintf(lineOut, sizeof(lineOut), "    type: %s\n", valStr);
            if (!rc_write(&out, lineOut)) ok = false;
          } else {
            // unknown per-switch attribute, or "type" on an unavailable
            // switch -- preserved unchanged.
            if (!rc_write(&out, nested) || !rc_write(&out, "\n")) ok = false;
          }
        }

        for (int i = 0; i < RC_MAX_SWITCHES && ok; i++) {
          if (!rc_switch_available(i) || switchEmitted[i]) continue;
          char valStr[RC_VAL_MAX];
          rc_switch_type_to_str(g_eeGeneral.switchType(i), valStr,
                                sizeof(valStr));
          snprintf(lineOut, sizeof(lineOut), "  %s:\n    type: %s\n",
                   rc_switch_keys[i], valStr);
          if (!rc_write(&out, lineOut)) ok = false;
        }
        continue;
      }

      int idx = rc_find_field(key, keyLen);
      if (idx < 0) {
        if (!rc_write(&out, line) || !rc_write(&out, "\n"))
          ok = false;  // unknown -- preserved verbatim
        continue;
      }

      const RadioConfigField& f = rc_fields[idx];
      emitted[idx] = true;

      if (!f.isAvailable()) {
        if (!rc_write(&out, line) || !rc_write(&out, "\n"))
          ok = false;  // known+unavailable -- preserved
        continue;
      }

      char valStr[RC_VAL_MAX];
      if (f.type == RcType::STR) {
        f.getStr(g_eeGeneral, valStr, sizeof(valStr));
      } else {
        rc_format_value(f, f.get(g_eeGeneral), valStr, sizeof(valStr));
      }
      snprintf(lineOut, sizeof(lineOut), "%s: %s\n", f.key, valStr);
      if (!rc_write(&out, lineOut)) ok = false;
    }
    f_close(&in);
  }

  if (ok && !switchesBlockHandled) {
    bool anyAvailable = false;
    for (int i = 0; i < RC_MAX_SWITCHES; i++) {
      if (rc_switch_available(i)) {
        anyAvailable = true;
        break;
      }
    }
    if (anyAvailable) {
      if (!rc_write(&out, "switches:\n")) ok = false;
      for (int i = 0; i < RC_MAX_SWITCHES && ok; i++) {
        if (!rc_switch_available(i)) continue;
        char valStr[RC_VAL_MAX];
        rc_switch_type_to_str(g_eeGeneral.switchType(i), valStr,
                              sizeof(valStr));
        snprintf(lineOut, sizeof(lineOut), "  %s:\n    type: %s\n",
                 rc_switch_keys[i], valStr);
        if (!rc_write(&out, lineOut)) ok = false;
      }
    }
  }

  if (ok) {
    for (size_t i = 0; i < rc_field_count; i++) {
      const RadioConfigField& f = rc_fields[i];
      if (!f.isAvailable() || emitted[i]) continue;
      char valStr[RC_VAL_MAX];
      if (f.type == RcType::STR) {
        f.getStr(g_eeGeneral, valStr, sizeof(valStr));
      } else {
        rc_format_value(f, f.get(g_eeGeneral), valStr, sizeof(valStr));
      }
      snprintf(lineOut, sizeof(lineOut), "%s: %s\n", f.key, valStr);
      if (!rc_write(&out, lineOut)) {
        ok = false;
        break;
      }
    }
  }

  // f_close() flushes any cached write data in this FatFs configuration.
  f_close(&out);

  if (!ok) {
    f_unlink(tmpPath);
    return SDCARD_ERROR(FR_DISK_ERR);
  }

  // Never truncate the active file before the replacement is ready: the
  // temporary file above is fully written & synced first.
  f_unlink(path);
  FRESULT r = f_rename(tmpPath, path);
  if (r != FR_OK) return SDCARD_ERROR(r);

  return nullptr;
}
