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

#include "hal/module_port.h"
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"

#include "edgetx.h"
#include "switches.h"
#include "mixes.h"
#include "os/sleep.h"

#undef CPN

uint8_t switchToMix(uint8_t source)
{
  div_t qr = div(source-1, 3);
  return qr.quot + MIXSRC_FIRST_SWITCH;
}

bool isInputAvailable(int input)
{
  for (int i=0; i<MAX_EXPOS; i++) {
    ExpoData * expo = expoAddress(i);
    if (!EXPO_VALID(expo))
      break;
    if (expo->chn == input)
      return true;
  }
  return false;
}

bool isVarioSensorAvailable(int sensor)
{
  if (sensor == 0)
    return true;
  else {
    return (isSensorAvailable(sensor) && (isSensorUnit(sensor, UNIT_METERS_PER_SECOND) || isSensorUnit(sensor, UNIT_FEET_PER_SECOND)));
  }
}

bool isSensorAvailable(int sensor)
{
  if (sensor == 0)
    return true;
  else
    return isTelemetryFieldAvailable(abs(sensor) - 1);
}

bool isSensorUnit(int sensor, uint8_t unit)
{
  if (sensor <= 0 || sensor > MAX_TELEMETRY_SENSORS ) {
    return true;
  }
  else {
    return g_model.telemetrySensors[sensor-1].unit == unit;
  }
}

bool isCellsSensor(int sensor)
{
  return isSensorUnit(sensor, UNIT_CELLS);
}

bool isGPSSensor(int sensor)
{
  return isSensorUnit(sensor, UNIT_GPS);
}

bool isAltSensor(int sensor)
{
  return isSensorUnit(sensor, UNIT_DIST) || isSensorUnit(sensor, UNIT_FEET);
}

bool isVoltsSensor(int sensor)
{
  return isSensorUnit(sensor, UNIT_VOLTS) || isSensorUnit(sensor, UNIT_CELLS);
}

bool isCurrentSensor(int sensor)
{
  return isSensorUnit(sensor, UNIT_AMPS);
}

bool isTelemetryFieldAvailable(int index)
{
  TelemetrySensor & sensor = g_model.telemetrySensors[index];
  return sensor.isAvailable();
}

uint8_t getTelemetrySensorsCount()
{
  uint8_t count = 0;
  for (auto telemetrySensor : g_model.telemetrySensors) {
    if (telemetrySensor.isAvailable()) {
      ++count;
    }
  }
  return count;
}

bool isTelemetryFieldComparisonAvailable(int index)
{
  if (!isTelemetryFieldAvailable(index))
    return false;

  TelemetrySensor & sensor = g_model.telemetrySensors[index];
  if (sensor.unit >= UNIT_DATETIME)
    return false;
  return true;
}

bool isChannelUsed(int channel)
{
  for (int i=0; i<MAX_MIXERS; ++i) {
    MixData *md = mixAddress(i);
    if (md->srcRaw == 0) return false;
    if (md->destCh == channel) return true;
    if (md->destCh > channel) return false;
  }
  return false;
}

int getChannelsUsed()
{
  int result = 0;
  int lastCh = -1;
  for (int i=0; i<MAX_MIXERS; ++i) {
    MixData *md = mixAddress(i);
    if (md->srcRaw == 0) return result;
    if (md->destCh != lastCh) { ++result; lastCh = md->destCh; }
  }
  return result;
}

static bool sourceIsAvailable(int source) { return true; }

static bool isSourceLuaAvailable(int source) {
#if defined(LUA_MODEL_SCRIPTS)
  if (modelCustomScriptsEnabled()) {
    div_t qr = div(source, MAX_SCRIPT_OUTPUTS);
    return (qr.rem < scriptInputsOutputs[qr.quot].outputsCount);
  }
#endif
  return false;
}

static bool isSourceStickAvailable(int source) {
  return source < adcGetMaxInputs(ADC_INPUT_MAIN);
}

static bool isSourcePotAvailable(int source) {
  return IS_POT_SLIDER_AVAILABLE(source);
}

#if defined(PCBHORUS)
static bool isSourceSpacemouseAvailable(int source) {
#if defined(SPACEMOUSE)
  return serialGetModePort(UART_MODE_SPACEMOUSE) >= 0;
#else
  return false;
#endif
}
#endif

static bool isSourceSwitchAvailable(int source) {
  return SWITCH_EXISTS(source);
}

#if defined(FUNCTION_SWITCHES)
static bool isSourceFuncSwitchAvailable(int source) {
  return getSwitchCountInFSGroup(source + 1) > 0;
}
#endif

static bool isSourceLSAvailable(int source) {
  LogicalSwitchData * cs = lswAddress(source);
  return (cs->func != LS_FUNC_NONE);
}

static bool isSourceGvarAvailable(int source) {
#if defined(GVARS)
  return modelGVEnabled();
#else
  return false;
#endif
}

static bool isSourceTimerAvailable(int source) {
  TimerData *timer = &g_model.timers[source];
  return timer->mode != 0;
}

static bool isSourceTelemAvailable(int source) {
  if (!modelTelemetryEnabled())
    return false;
  div_t qr = div(source, 3);
  if (qr.rem == 0)
    return isTelemetryFieldAvailable(qr.quot);
  else
    return isTelemetryFieldComparisonAvailable(qr.quot);
}

struct sourceAvailableCheck {
  uint16_t first;
  uint16_t last;
  SrcTypes type;
  bool (*check)(int);
};

static struct sourceAvailableCheck sourceChecks[] = {
  { MIXSRC_FIRST_INPUT, MIXSRC_LAST_INPUT, SRC_INPUT, isInputAvailable },
  { MIXSRC_FIRST_LUA, MIXSRC_LAST_LUA, SRC_LUA, isSourceLuaAvailable },
  { MIXSRC_FIRST_STICK, MIXSRC_LAST_STICK, SRC_STICK, isSourceStickAvailable },
  { MIXSRC_FIRST_POT, MIXSRC_LAST_POT, SRC_POT, isSourcePotAvailable },
#if defined(IMU)
  { MIXSRC_TILT_X, MIXSRC_TILT_Y, SRC_TILT, sourceIsAvailable },
#endif
#if defined(LUMINOSITY_SENSOR)
  { MIXSRC_LIGHT, MIXSRC_LIGHT, SRC_LIGHT, sourceIsAvailable },
#endif
#if defined(PCBHORUS)
  { MIXSRC_FIRST_SPACEMOUSE, MIXSRC_LAST_SPACEMOUSE, SRC_SPACEMOUSE, isSourceSpacemouseAvailable },
#endif
  { MIXSRC_MIN, MIXSRC_MAX, SRC_MINMAX, sourceIsAvailable },
  { MIXSRC_FIRST_SWITCH, MIXSRC_LAST_SWITCH, SRC_SWITCH, isSourceSwitchAvailable },
#if defined(FUNCTION_SWITCHES)
  { MIXSRC_FIRST_CUSTOMSWITCH_GROUP, MIXSRC_LAST_CUSTOMSWITCH_GROUP, SRC_FUNC_SWITCH, isSourceFuncSwitchAvailable },
#endif
  { MIXSRC_FIRST_LOGICAL_SWITCH, MIXSRC_LAST_LOGICAL_SWITCH, SRC_LOGICAL_SWITCH, isSourceLSAvailable },
  { MIXSRC_FIRST_CH, MIXSRC_LAST_CH, SRC_CHANNEL, isChannelUsed },
  { MIXSRC_FIRST_CH, MIXSRC_LAST_CH, SRC_CHANNEL_ALL, sourceIsAvailable },
  { MIXSRC_FIRST_GVAR, MIXSRC_LAST_GVAR, SRC_GVAR, isSourceGvarAvailable },
  { MIXSRC_TX_VOLTAGE, MIXSRC_TX_GPS, SRC_TX, sourceIsAvailable },
  { MIXSRC_FIRST_TIMER, MIXSRC_LAST_TIMER, SRC_TIMER, isSourceTimerAvailable },
  { MIXSRC_FIRST_TELEM, MIXSRC_LAST_TELEM, SRC_TELEM, isSourceTelemAvailable },
  { MIXSRC_NONE, MIXSRC_NONE, SRC_NONE, sourceIsAvailable },
};

bool checkSourceAvailable(int source, uint32_t sourceTypes)
{
  if (source < 0)
    source = -source;

  for (size_t i = 0 ; i < DIM(sourceChecks); i += 1) {
    if (sourceChecks[i].type & sourceTypes && source >= sourceChecks[i].first && source <= sourceChecks[i].last) {
      return sourceChecks[i].check(source - sourceChecks[i].first);
    }
  }

  return false;
}

#define SRC_COMMON \
            SRC_STICK | SRC_POT | SRC_TILT | SRC_LIGHT | SRC_SPACEMOUSE | SRC_MINMAX | \
            SRC_SWITCH | SRC_FUNC_SWITCH | SRC_LOGICAL_SWITCH | SRC_GVAR

bool isSourceAvailable(int source)
{
  return checkSourceAvailable(source,
            SRC_COMMON | SRC_INPUT | SRC_LUA | SRC_CHANNEL | SRC_TX | SRC_TIMER | SRC_TELEM | SRC_NONE
            );
}

bool isSourceAvailableForBacklightOrVolume(int source)
{
  return checkSourceAvailable(source, SRC_SWITCH | SRC_POT | SRC_LIGHT | SRC_NONE);
}

bool isLogicalSwitchAvailable(int index)
{
  LogicalSwitchData * lsw = lswAddress(index);
  return (lsw->func != LS_FUNC_NONE);
}

bool isSwitchAvailable(int swtch, SwitchContext context)
{
  bool negative = false;
  (void)negative;

  if (swtch < 0) {
    if (swtch == -SWSRC_ON || swtch == -SWSRC_ONE) {
      return false;
    }
    negative = true;
    swtch = -swtch;
  }

  if (swtch >= SWSRC_FIRST_SWITCH && swtch <= SWSRC_LAST_SWITCH) {
    div_t swinfo = switchInfo(swtch);
    if (swinfo.quot >= switchGetMaxAllSwitches()) {
      return false;
    }

    if (!SWITCH_EXISTS(swinfo.quot)) {
      return false;
    }

    if (switchIsCustomSwitch(swinfo.quot) && context == GeneralCustomFunctionsContext) {
      return false;   // FS are defined at model level, and cannot be in global functions
    }

    if (!IS_CONFIG_3POS(swinfo.quot)) {
      if (swinfo.rem == 1) {
        // mid position not available for 2POS switches
        return false;
      }
    }
    return true;
  }

  if (swtch >= SWSRC_FIRST_MULTIPOS_SWITCH && swtch <= SWSRC_LAST_MULTIPOS_SWITCH) {
    int index = (swtch - SWSRC_FIRST_MULTIPOS_SWITCH) / XPOTS_MULTIPOS_COUNT;
    return (index < adcGetMaxInputs(ADC_INPUT_FLEX)) ? IS_POT_MULTIPOS(index) : false;
  }

  if (swtch >= SWSRC_FIRST_TRIM && swtch <= SWSRC_LAST_TRIM) {
    int index = (swtch - SWSRC_FIRST_TRIM) / 2;
    return index < keysGetMaxTrims();
  }

  if (swtch >= SWSRC_FIRST_LOGICAL_SWITCH && swtch <= SWSRC_LAST_LOGICAL_SWITCH) {
    if (context == GeneralCustomFunctionsContext) {
      return false;
    }
    else if (context != LogicalSwitchesContext) {
      return isLogicalSwitchAvailable(swtch - SWSRC_FIRST_LOGICAL_SWITCH);
    }
  }

  if (context != ModelCustomFunctionsContext && context != GeneralCustomFunctionsContext && (swtch == SWSRC_ON || swtch == SWSRC_ONE)) {
    return false;
  }

  if (swtch >= SWSRC_FIRST_SENSOR && swtch <= SWSRC_LAST_SENSOR) {
    if (context == GeneralCustomFunctionsContext)
      return false;
    else
      return isTelemetryFieldAvailable(swtch - SWSRC_FIRST_SENSOR);
  }

  return true;
}

static bool switchIsAvailable(int swtch, bool invert)
{
  return true;
}

static bool isSwitchSwitchAvailable(int swtch, bool invert) {
  // Check normal switch
  if (swtch < MAX_SWITCHES * 3) {
    div_t swinfo = switchInfo(swtch + SWSRC_FIRST_SWITCH);
    if (swinfo.quot >= switchGetMaxAllSwitches()) {
      return false;
    }

    if (!SWITCH_EXISTS(swinfo.quot)) {
      return false;
    }

    if (!IS_CONFIG_3POS(swinfo.quot)) {
      if (swinfo.rem == 1) {
        // mid position not available for 2POS switches
        return false;
      }
    }

    return true;
  }

  // Multipos switch
  int index = (swtch + SWSRC_FIRST_SWITCH - SWSRC_FIRST_MULTIPOS_SWITCH) / XPOTS_MULTIPOS_COUNT;
  return (index < adcGetMaxInputs(ADC_INPUT_FLEX)) ? IS_POT_MULTIPOS(index) : false;
}

static bool isSwitchTrimAvailable(int swtch, bool invert) {
  int index = swtch / 2;
  return index < keysGetMaxTrims();
}

static bool isSwitchLSAvailable(int swtch, bool invert) {
  return isLogicalSwitchAvailable(swtch);
}

static bool isSwitchTelemAvailable(int swtch, bool invert) {
  return isTelemetryFieldAvailable(swtch);
}

static bool isSwitchOtherAvailable(int swtch, bool invert) {
  swtch += SWSRC_ON;
  if (invert && (swtch == SWSRC_ON || swtch == SWSRC_ONE))
    return false;
  if (swtch == SWSRC_ON || swtch == SWSRC_ONE || swtch == SWSRC_TELEMETRY_STREAMING ||
      swtch == SWSRC_RADIO_ACTIVITY)
    return true;
#if defined(DEBUG_LATENCY)
  if (swtch == SWSRC_LATENCY_TOGGLE)
    return true;
#endif
  return false;
}

struct switchAvailableCheck {
  uint16_t first;
  uint16_t last;
  SwitchTypes type;
  bool (*check)(int, bool);
};

static struct switchAvailableCheck switchChecks[] = {
  { SWSRC_FIRST_SWITCH, SWSRC_LAST_MULTIPOS_SWITCH, SW_SWITCH, isSwitchSwitchAvailable },
  { SWSRC_FIRST_TRIM, SWSRC_LAST_TRIM, SW_TRIM, isSwitchTrimAvailable },
  { SWSRC_FIRST_LOGICAL_SWITCH, SWSRC_LAST_LOGICAL_SWITCH, SW_LOGICAL_SWITCH, isSwitchLSAvailable },
  { SWSRC_FIRST_SENSOR, SWSRC_LAST_SENSOR, SW_TELEM, isSwitchTelemAvailable },
  { SWSRC_ON, SWSRC_COUNT - 1, SW_OTHER, isSwitchOtherAvailable },
  { SWSRC_NONE, SWSRC_NONE, SW_NONE, switchIsAvailable },
};

bool checkSwitchAvailable(int swtch, uint32_t swtchTypes)
{
  bool invert = false;
  if (swtch < 0) {
    swtch = -swtch;
    invert = true;
  }

  for (size_t i = 0 ; i < DIM(switchChecks); i += 1) {
    if (switchChecks[i].type & swtchTypes && swtch >= switchChecks[i].first && swtch <= switchChecks[i].last) {
      return switchChecks[i].check(swtch - switchChecks[i].first, invert);
    }
  }

  return false;
}

bool isSerialModeAvailable(uint8_t port_nr, int mode)
{
  if (mode == UART_MODE_RESERVED_TELEMETRY || mode == UART_MODE_RESERVED_3 ||
      mode == UART_MODE_RESERVED_4) return false;
#if defined(USB_SERIAL)
  // Do not list OFF on VCP if internal RF module is set to CROSSFIRE to allow pass-through flashing
  if (port_nr == SP_VCP && mode == UART_MODE_NONE && isInternalModuleCrossfire())
    return false;
#endif

  if (mode == UART_MODE_NONE)
    return true;

#if !defined(DEBUG)
  if (mode == UART_MODE_DEBUG)
    return false;
#endif

#if !defined(CLI)
  if (mode == UART_MODE_CLI) return false;
#else
  // CLI is only supported on VCP
  if (port_nr != SP_VCP && mode == UART_MODE_CLI) return false;
#endif

#if !defined(INTERNAL_GPS)
  if (mode == UART_MODE_GPS)
    return false;
#elif defined(USB_SERIAL)
  // GPS is not supported on VCP
  if (port_nr == SP_VCP && mode == UART_MODE_GPS)
    return false;
#endif

#if !defined(SPACEMOUSE)
  if (mode == UART_MODE_SPACEMOUSE)
    return false;
#elif defined(USB_SERIAL)
  // SPACEMOUSE is not supported on VCP
  if (port_nr == SP_VCP && mode == UART_MODE_SPACEMOUSE)
    return false;
#endif

#if !defined(AUX_SERIAL_DMA_TX) || defined(EXTMODULE_USART)
  if (mode == UART_MODE_EXT_MODULE)
    return false;
#else // defined(AUX_SERIAL_DMA_TX) && !defined(EXTMODULE_USART)
  // UART_MODE_EXT_MODULE is only supported on AUX1, as AUX2 has no TX DMA
  if (mode == UART_MODE_EXT_MODULE && port_nr != SP_AUX1)
    return false;
#endif

#if !defined(LUA)
  if (mode == UART_MODE_LUA)
    return false;
#endif

  auto p = serialGetModePort(mode);
  if (p >= 0 && p != port_nr) return false;
  return true;
}

bool isSwitchAvailableInLogicalSwitches(int swtch)
{
  return isSwitchAvailable(swtch, LogicalSwitchesContext);
}

bool isSwitchAvailableInMixes(int swtch)
{
  return isSwitchAvailable(swtch, MixesContext);
}

bool isSwitchAvailableForArming(int swtch)
{
  return isSwitchAvailable(swtch, ModelCustomFunctionsContext);
}

#if defined(COLORLCD)
bool isSwitch2POSWarningStateAvailable(int state)
{
  return (state != 2); // two pos switch - middle state not available
}
#endif // #if defined(COLORLCD)

bool isThrottleSourceAvailable(int src)
{
#if !defined(COLORLCD)
  src = throttleSource2Source(src);
#endif
  return isSourceAvailable(src) &&
    ((src == MIXSRC_FIRST_STICK + inputMappingGetThrottle()) ||
     ((src >= MIXSRC_FIRST_POT) && (src <= MIXSRC_LAST_POT)) ||
     ((src >= MIXSRC_FIRST_CH) && (src <= MIXSRC_LAST_CH)));
}

bool isAssignableFunctionAvailable(int function, bool modelFunctions)
{
  switch (function) {
    case FUNC_RESERVED_TRIM:
      return false;
    case FUNC_OVERRIDE_CHANNEL:
#if defined(OVERRIDE_CHANNEL_FUNCTION)
      return modelFunctions;
#else
      return false;
#endif
    case FUNC_ADJUST_GVAR:
#if defined(GVARS)
      return modelFunctions;
#else
      return false;
#endif
#if !defined(HAPTIC)
    case FUNC_HAPTIC:
      return false;
#endif
#if !defined(DANGEROUS_MODULE_FUNCTIONS)
    case FUNC_BIND:
      return false;
#endif
#if !defined(LUA)
    case FUNC_PLAY_SCRIPT:
      return false;
#endif
#if !defined(AUDIO_MUTE_GPIO)
    case FUNC_DISABLE_AUDIO_AMP:
      return false;
#endif
#if !defined(LED_STRIP_LENGTH)
    case FUNC_RGB_LED:
      return false;
#endif
#if !defined(DEBUG)
    case FUNC_TEST:
      return false;
#endif
#if defined(FUNCTION_SWITCHES) || defined(CFN_ONLY)
    case FUNC_PUSH_CUST_SWITCH:
      return modelFunctions;
#endif
    case FUNC_DISABLE_KEYS:
#if defined(KEYS_LOCK_KEY1) && defined(KEYS_LOCK_KEY2)
      return g_eeGeneral.keyLockEnabled;
#else
      return false;
#endif

    default:
      return true;
  }
}

#if !defined(COLORLCD)
bool isAssignableFunctionAvailable(int function)
{
  return isAssignableFunctionAvailable(function, menuHandlers[menuLevel] == menuModelSpecialFunctions);
}
#endif

int timersSetupCount()
{
  int tc = 0;
  for (int i = 0; i < MAX_TIMERS; i += 1)
    if (isTimerSourceAvailable(i))
      tc += 1;
  return tc;
}

bool isTimerSourceAvailable(int index)
{
  TimerData *timer = &g_model.timers[index];
  return timer->mode != 0;
}

bool isSourceAvailableInGlobalResetSpecialFunction(int index)
{
  if (index >= FUNC_RESET_PARAM_FIRST_TELEM)
    return false;
  else
    return isSourceAvailableInResetSpecialFunction(index);
}

bool isSourceAvailableInResetSpecialFunction(int index)
{
  if (index == FUNC_RESET_RESERVED_TRIMS) return false;
  if (index >= FUNC_RESET_PARAM_FIRST_TELEM) {
    TelemetrySensor & telemetrySensor = g_model.telemetrySensors[index-FUNC_RESET_PARAM_FIRST_TELEM];
    return telemetrySensor.isAvailable();
  }
  else if (index <= FUNC_RESET_TIMER3) {
    if (index > (TIMERS - 1))
      return false;
    else {
      TimerData *timer = &g_model.timers[index];
      return timer->mode != 0;
    }
  }
  else {
    return true;
  }
}

#if defined(EXTERNAL_ANTENNA)

#if defined(COLORLCD)

#include "menu.h"
#include "mainwindow.h"

class AntennaSelectionMenu : public Menu
{
 public:
  AntennaSelectionMenu() : Menu()
  {
    setTitle(STR_ANTENNA);
    addLine(STR_USE_INTERNAL_ANTENNA,
            [] { globalData.externalAntennaEnabled = false; });
    addLine(STR_USE_EXTERNAL_ANTENNA,
            [] { globalData.externalAntennaEnabled = true; });
    setCloseWhenClickOutside(false);
  }

 protected:
  void onCancel() override {}
};

static void runAntennaSelectionMenu()
{
  auto menu = new AntennaSelectionMenu();

  MainWindow::instance()->blockUntilClose(true, [=]() {
    return menu->deleted();
  });
}
#else // !COLORLCD

#if defined(EXTERNAL_ANTENNA)
void onAntennaSelection(const char* result)
{
  if (result == STR_USE_INTERNAL_ANTENNA) {
    globalData.externalAntennaEnabled = false;
  } else if (result == STR_USE_EXTERNAL_ANTENNA) {
    globalData.externalAntennaEnabled = true;
  } else {
    checkExternalAntenna();
  }
}

void onAntennaSwitchConfirm(const char * result)
{
  if (result == STR_OK) {
    // Switch to external antenna confirmation
    globalData.externalAntennaEnabled = true;
  }
}
#endif // defined(EXTERNAL_ANTENNA)

#endif // defined(COLORLCD)

void checkExternalAntenna()
{
  // Get the per-model antenna mode from the appropriate field
  int8_t modelAntennaMode = g_model.moduleData[INTERNAL_MODULE].antennaMode;

  if (g_eeGeneral.antennaMode == ANTENNA_MODE_EXTERNAL) {
    globalData.externalAntennaEnabled = true;
#if defined(INTMODULE_ANTSEL_GPIO) && !defined(SIMU)
    INTMODULE_ANTSEL_EXT();
    LED_ERROR_BEGIN();
    RAISE_ALERT(STR_ANTENNACONFIRM1, STR_ANTENNACONFIRM2, STR_PRESS_ANY_KEY_TO_SKIP, AU_WARNING1);
#endif
  } else if (g_eeGeneral.antennaMode == ANTENNA_MODE_PER_MODEL &&
             modelAntennaMode == ANTENNA_MODE_EXTERNAL) {
    if (!globalData.externalAntennaEnabled) {
#if defined(COLORLCD)
      if (confirmationDialog(STR_ANTENNACONFIRM1, STR_ANTENNACONFIRM2)) {
        globalData.externalAntennaEnabled = true;
      }
#elif defined(EXTERNAL_ANTENNA)
      POPUP_CONFIRMATION(STR_ANTENNACONFIRM1, onAntennaSwitchConfirm);
      SET_WARNING_INFO(STR_ANTENNACONFIRM2, strlen(STR_ANTENNACONFIRM2), 0);
#endif
    }
  } else if (g_eeGeneral.antennaMode == ANTENNA_MODE_ASK ||
             (g_eeGeneral.antennaMode == ANTENNA_MODE_PER_MODEL &&
              modelAntennaMode == ANTENNA_MODE_ASK)) {
    globalData.externalAntennaEnabled = false;
#if defined(COLORLCD)
    runAntennaSelectionMenu();
#elif defined(EXTERNAL_ANTENNA)
    POPUP_MENU_START(onAntennaSelection, 2, STR_USE_INTERNAL_ANTENNA, STR_USE_EXTERNAL_ANTENNA);
#endif
  } else {
    globalData.externalAntennaEnabled = false;
  }

#if defined(INTMODULE_ANTSEL_GPIO) && !defined(SIMU)
  // Apply hardware GPIO switch based on result
  if (g_eeGeneral.antennaMode != ANTENNA_MODE_EXTERNAL) {
    if (globalData.externalAntennaEnabled) {
      INTMODULE_ANTSEL_EXT();
    } else {
      INTMODULE_ANTSEL_INT();
    }
  }
#endif
}

#if defined(COLORLCD)
// Shared by every antenna-mode Choice widget (radio-level and per-model,
// GPIO and non-GPIO boards) so the "apply to hardware" step can't be
// forgotten in just one of the call sites.
void setAntennaModeWithConfirm(int8_t newMode, uint8_t storageId,
                                std::function<void(int8_t)> setter)
{
  int8_t modelAntennaMode = g_model.moduleData[INTERNAL_MODULE].antennaMode;
  bool willEnableExternal =
      newMode == ANTENNA_MODE_EXTERNAL ||
      (newMode == ANTENNA_MODE_PER_MODEL && modelAntennaMode == ANTENNA_MODE_EXTERNAL);

  if (!isExternalAntennaEnabled() && willEnableExternal) {
    if (confirmationDialog(STR_ANTENNACONFIRM1, STR_ANTENNACONFIRM2)) {
      setter(newMode);
      storageDirty(storageId);
      // Consent already obtained above; mark enabled so checkExternalAntenna()
      // applies the GPIO instead of asking again.
      globalData.externalAntennaEnabled = true;
      checkExternalAntenna();
    }
  } else {
    setter(newMode);
    storageDirty(storageId);
    checkExternalAntenna();
  }
}
#endif // defined(COLORLCD)

#endif // defined(EXTERNAL_ANTENNA)

bool isInternalModuleSupported(int moduleType)
{
  if (moduleType == MODULE_TYPE_NONE) return true;
#if defined(INTERNAL_MODULE_CRSF)
  return moduleType == MODULE_TYPE_CROSSFIRE;
#else
  return false;
#endif
}

bool isInternalModuleAvailable(int moduleType)
{
  if (moduleType == MODULE_TYPE_NONE) return true;
#if defined(HARDWARE_INTERNAL_MODULE)
#if defined(MUTUALLY_EXCLUSIVE_MODULES)
  if (isModuleCrossfire(EXTERNAL_MODULE)) return false;
#endif
  return isInternalModuleSupported(moduleType) &&
         g_eeGeneral.internalModule == moduleType;
#else
  return false;
#endif
}

bool isExternalModuleAvailable(int moduleType)
{
  if (moduleType == MODULE_TYPE_NONE) return true;
#if defined(HARDWARE_EXTERNAL_MODULE)
#if defined(MUTUALLY_EXCLUSIVE_MODULES)
  if (isModuleCrossfire(INTERNAL_MODULE)) return false;
#endif
  return moduleType == MODULE_TYPE_CROSSFIRE;
#else
  return false;
#endif
}

bool modelHasNotes()
{
  char filename[sizeof(MODELS_PATH)+1+LEN_MODEL_NAME*3+sizeof(TEXT_EXT)] = MODELS_PATH "/";
  char *buf = strcat_currentmodelname(&filename[sizeof(MODELS_PATH)], 0);
  strcpy(buf, TEXT_EXT);
  if (isFileAvailable(filename)) {
    return true;
  }

  buf = strcat_currentmodelname(&filename[sizeof(MODELS_PATH)], ' ');
  strcpy(buf, TEXT_EXT);
  if (isFileAvailable(filename)) {
    return true;
  }

#if defined(STORAGE_MODELSLIST)
  buf = strAppendFilename(&filename[sizeof(MODELS_PATH)],
                          g_eeGeneral.currModelFilename, LEN_MODEL_FILENAME);
  strcpy(buf, TEXT_EXT);
  if (isFileAvailable(filename)) {
    return true;
  }
#endif

  return false;
}

bool confirmModelChange()
{
  if (TELEMETRY_STREAMING()) {
    RAISE_ALERT(STR_MODEL, STR_MODEL_STILL_POWERED, STR_PRESS_ENTER_TO_CONFIRM, AU_MODEL_STILL_POWERED);
    while (TELEMETRY_STREAMING()) {
      sleep_ms(20);
      if (readKeys() == (1 << KEY_ENTER)) {
        killEvents(KEY_ENTER);
        return true;
      }
      else if (readKeys() == (1 << KEY_EXIT)) {
        killEvents(KEY_EXIT);
        return false;
      }
    }
  }
  return true;
}

int getFirstAvailable(int min, int max, IsValueAvailable isValueAvailable)
{
  int retval = 0;
  for (int i = min; i <= max; i++) {
    if (isValueAvailable(i)) {
      retval = i;
      break;
    }
  }
  return retval;
}

#if !defined(COLORLCD)
uint8_t expandableSection(coord_t y, const char* title, uint8_t value, uint8_t attr, event_t event)
{
  lcdDrawTextAlignedLeft(y, title);
  lcdDrawText(LCD_W == 128 ? 120 : 200, y, value ? CHAR_UP : CHAR_DOWN, attr);
  if (attr && (event == EVT_KEY_BREAK(KEY_ENTER))) {
    value = !value;
    s_editMode = 0;
  }
  return value;
}
#endif

bool isPotTypeAvailable(uint8_t type)
{
  if (type == FLEX_SWITCH) {
    if (MAX_FLEX_SWITCHES == 0)
      return false;

    auto availableFlexSwitch = MAX_FLEX_SWITCHES;
    for (uint8_t i = 0; i < adcGetMaxInputs(ADC_INPUT_FLEX); i++) {
      if (POT_CONFIG(i) == FLEX_SWITCH) availableFlexSwitch--;
      if (availableFlexSwitch == 0) return false;
    }
  }

  return true;
}

bool isFlexSwitchSourceValid(int source)
{
  if (MAX_FLEX_SWITCHES == 0) return false;

  // Allow NONE
  if (source < 0) return true;

  // already assigned ?
  for (int i=0; i < MAX_FLEX_SWITCHES;i++) {
    if (source == switchGetFlexConfig_raw(i))
      return false;
  }

  if (POT_CONFIG(source) != FLEX_SWITCH) return false;

  return true;
}

bool getStickInversion(int index)
{
  return bfGet<uint8_t>(g_eeGeneral.stickInvert, index, STICK_CFG_INV_BITS);
}

void setStickInversion(int index, bool value)
{
  g_eeGeneral.stickInvert = bfSet<uint8_t>(g_eeGeneral.stickInvert, value, index, STICK_CFG_INV_BITS);
}

bool getPotInversion(int index)
{
  return bfGet<potconfig_t>(g_eeGeneral.potsConfig, (POT_CFG_BITS * index) + POT_CFG_TYPE_BITS, POT_CFG_INV_BITS);
}

void setPotInversion(int index, bool value)
{
  g_eeGeneral.potsConfig = bfSet<potconfig_t>(g_eeGeneral.potsConfig, value, (POT_CFG_BITS * index) + POT_CFG_TYPE_BITS, POT_CFG_INV_BITS);
}

uint8_t getPotType(int index)
{
  return bfGet<potconfig_t>(g_eeGeneral.potsConfig, POT_CFG_BITS * index, POT_CFG_TYPE_BITS);
}

void setPotType(int index, int value)
{
  g_eeGeneral.potsConfig = bfSet<potconfig_t>(g_eeGeneral.potsConfig, value, (POT_CFG_BITS * index), POT_CFG_TYPE_BITS);
}

uint8_t MODULE_BIND_ROWS(int moduleIdx)
{
  if (!isModuleCrossfire(moduleIdx)) return HIDDEN_ROW;
  return isModuleBindAvailable(moduleIdx) ? 1 : 0;
}

uint8_t MODULE_CHANNELS_ROWS(int moduleIdx)
{
  return isModuleCrossfire(moduleIdx) ? (uint8_t)0 : HIDDEN_ROW;
}
