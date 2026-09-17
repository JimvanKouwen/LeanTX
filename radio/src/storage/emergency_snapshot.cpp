#include "edgetx.h"
#include "emergency_snapshot.h"
#include "hal/switch_driver.h"

// These limits belong to the RTC format, not to storage. A runtime expansion
// requires an explicit snapshot review rather than silently changing the format.
static_assert(MAX_CALIB_ANALOG_INPUTS <= 20 && MAX_SWITCHES <= 20 && MAX_FLEX_SWITCHES <= 20, "RTC controls");
static_assert(MAX_OUTPUT_CHANNELS <= 32, "RTC channel capacity");
static_assert(MAX_OUTPUT_CHANNELS <= 32 && NUM_MODULES == 2, "RTC RF capacity");
#if defined(FUNCTION_SWITCHES)
static_assert(NUM_FUNCTIONS_SWITCHES <= 8, "RTC function switches");
#endif
static_assert(sizeof(potconfig_t) <= sizeof(uint64_t), "RTC pot configuration");

void captureEmergencySnapshot(EmergencySnapshot &s)
{
  memset(&s, 0, sizeof(s));
  s.radio.stickMode = g_eeGeneral.stickMode;
  s.radio.stickInvert = g_eeGeneral.stickInvert;
  s.radio.internalModule = g_eeGeneral.internalModule;
  s.radio.internalModuleBaudrate = g_eeGeneral.internalModuleBaudrate;
  s.radio.noJitterFilter = g_eeGeneral.noJitterFilter;
  s.radio.antennaMode = g_eeGeneral.antennaMode;
  s.radio.switchesDelay = g_eeGeneral.switchesDelay;
  s.radio.backlightMode = g_eeGeneral.backlightMode;
  s.radio.backlightBright = g_eeGeneral.backlightBright;
  s.radio.lightAutoOff = g_eeGeneral.lightAutoOff;
  s.radio.potsConfig = g_eeGeneral.potsConfig;
  for (unsigned i = 0; i < MAX_FLEX_SWITCHES; ++i)
    s.radio.flexChannels[i] = switchGetFlexConfig_raw(i);
#if !defined(PCBHORUS)
  s.radio.contrast = g_eeGeneral.contrast;
#endif
#if LCD_W == 128
  s.radio.invertLCD = g_eeGeneral.invertLCD;
#endif
#if defined(STICK_DEAD_ZONE)
  s.radio.stickDeadZone = g_eeGeneral.stickDeadZone;
#endif
#if defined(STM32F4)
  s.radio.uartSampleMode = g_eeGeneral.uartSampleMode;
#endif
#if defined(IMU)
  s.radio.imuMax = g_eeGeneral.imuMax;
#endif
#if defined(IMU)
  s.radio.imuOffset = g_eeGeneral.imuOffset;
#endif
#if defined(IMU)
  s.radio.imuInvert = g_eeGeneral.imuInvert;
#endif
  for (unsigned i = 0; i < MAX_CALIB_ANALOG_INPUTS; ++i) {
    s.radio.calibration[i][0] = g_eeGeneral.calib[i].mid;
    s.radio.calibration[i][1] = g_eeGeneral.calib[i].spanNeg;
    s.radio.calibration[i][2] = g_eeGeneral.calib[i].spanPos;
  }
  for (unsigned i = 0; i < MAX_SWITCHES; ++i) {
    s.radio.switches[i].type = g_eeGeneral.switchConfig[i].type;
#if defined(FUNCTION_SWITCHES)
    s.radio.switches[i].start = g_eeGeneral.switchConfig[i].start;
#endif
  }
  s.model.jitterFilter = g_model.jitterFilter;
  for (unsigned i = 0; i < MAX_OUTPUT_CHANNELS; ++i)
    s.model.physicalInputs[i] = uint8_t(g_model.channelMappings[i].source);

  for (unsigned i = 0; i < NUM_MODULES; ++i) {
    s.model.modules[i].type = g_model.moduleData[i].type;
    s.model.modules[i].channelsStart = g_model.moduleData[i].channelsStart;
    s.model.modules[i].channelsCount = g_model.moduleData[i].channelsCount;
    s.model.modules[i].antennaMode = g_model.moduleData[i].antennaMode;
    s.model.modules[i].telemetryBaudrate = g_model.moduleData[i].crsf.telemetryBaudrate;
    s.model.modules[i].crsfArmingMode = g_model.moduleData[i].crsf.crsfArmingMode;
    s.model.modules[i].crsfArmingCondition = g_model.moduleData[i].crsf.crsfArmingCondition;
  }
  for (unsigned i = 0; i < NUM_MODULES; ++i) {
    s.model.modelId[i] = g_model.header.modelId[i];
  }
#if defined(FUNCTION_SWITCHES)
  for (unsigned i = 0; i < NUM_FUNCTIONS_SWITCHES; ++i) {
    s.model.switches[i].type = g_model.customSwitches[i].type;
    s.model.switches[i].start = g_model.customSwitches[i].start;
    s.model.switches[i].group = g_model.customSwitches[i].group;
    s.model.switches[i].state = g_model.customSwitches[i].state;
  }
  s.model.cfsGroupOn = g_model.cfsGroupOn;
#endif
}

void restoreEmergencySnapshot(const EmergencySnapshot &s)
{
  memset(&g_eeGeneral, 0, sizeof(g_eeGeneral));
  memset(&g_model, 0, sizeof(g_model));
  g_eeGeneral.stickMode = s.radio.stickMode;
  g_eeGeneral.stickInvert = s.radio.stickInvert;
  g_eeGeneral.internalModule = s.radio.internalModule;
  g_eeGeneral.internalModuleBaudrate = s.radio.internalModuleBaudrate;
  g_eeGeneral.noJitterFilter = s.radio.noJitterFilter;
  g_eeGeneral.antennaMode = s.radio.antennaMode;
  g_eeGeneral.switchesDelay = s.radio.switchesDelay;
  g_eeGeneral.backlightMode = s.radio.backlightMode;
  g_eeGeneral.backlightBright = s.radio.backlightBright;
  g_eeGeneral.lightAutoOff = s.radio.lightAutoOff;
  g_eeGeneral.potsConfig = s.radio.potsConfig;
  for (unsigned i = 0; i < MAX_FLEX_SWITCHES; ++i)
    switchConfigFlex_raw(i, s.radio.flexChannels[i]);
#if !defined(PCBHORUS)
  g_eeGeneral.contrast = s.radio.contrast;
#endif
#if LCD_W == 128
  g_eeGeneral.invertLCD = s.radio.invertLCD;
#endif
#if defined(STICK_DEAD_ZONE)
  g_eeGeneral.stickDeadZone = s.radio.stickDeadZone;
#endif
#if defined(STM32F4)
  g_eeGeneral.uartSampleMode = s.radio.uartSampleMode;
#endif
#if defined(IMU)
  g_eeGeneral.imuMax = s.radio.imuMax;
#endif
#if defined(IMU)
  g_eeGeneral.imuOffset = s.radio.imuOffset;
#endif
#if defined(IMU)
  g_eeGeneral.imuInvert = s.radio.imuInvert;
#endif
  for (unsigned i = 0; i < MAX_CALIB_ANALOG_INPUTS; ++i) {
    g_eeGeneral.calib[i].mid = s.radio.calibration[i][0];
    g_eeGeneral.calib[i].spanNeg = s.radio.calibration[i][1];
    g_eeGeneral.calib[i].spanPos = s.radio.calibration[i][2];
  }
  for (unsigned i = 0; i < MAX_SWITCHES; ++i) {
    g_eeGeneral.switchConfig[i].type = s.radio.switches[i].type;
#if defined(FUNCTION_SWITCHES)
    g_eeGeneral.switchConfig[i].start = s.radio.switches[i].start;
#endif
  }
  g_model.jitterFilter = s.model.jitterFilter;
  for (unsigned i = 0; i < MAX_OUTPUT_CHANNELS; ++i)
    g_model.channelMappings[i].source = PhysicalInputId(s.model.physicalInputs[i]);

  for (unsigned i = 0; i < NUM_MODULES; ++i) {
    g_model.moduleData[i].type = s.model.modules[i].type;
    g_model.moduleData[i].channelsStart = s.model.modules[i].channelsStart;
    g_model.moduleData[i].channelsCount = s.model.modules[i].channelsCount;
    g_model.moduleData[i].antennaMode = s.model.modules[i].antennaMode;
    g_model.moduleData[i].crsf.telemetryBaudrate = s.model.modules[i].telemetryBaudrate;
    g_model.moduleData[i].crsf.crsfArmingMode = s.model.modules[i].crsfArmingMode;
    g_model.moduleData[i].crsf.crsfArmingCondition = s.model.modules[i].crsfArmingCondition;
  }
  for (unsigned i = 0; i < NUM_MODULES; ++i) {
    g_model.header.modelId[i] = s.model.modelId[i];
  }
#if defined(FUNCTION_SWITCHES)
  for (unsigned i = 0; i < NUM_FUNCTIONS_SWITCHES; ++i) {
    g_model.customSwitches[i].type = s.model.switches[i].type;
    g_model.customSwitches[i].start = s.model.switches[i].start;
    g_model.customSwitches[i].group = s.model.switches[i].group;
    g_model.customSwitches[i].state = s.model.switches[i].state;
  }
  g_model.cfsGroupOn = s.model.cfsGroupOn;
#endif
}
