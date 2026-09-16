#include "edgetx.h"
#include "emergency_snapshot.h"
#include "hal/switch_driver.h"

// These limits belong to the RTC format, not to storage. A runtime expansion
// requires an explicit snapshot review rather than silently changing the format.
static_assert(MAX_CALIB_ANALOG_INPUTS <= 20 && MAX_SWITCHES <= 20 && MAX_FLEX_SWITCHES <= 20, "RTC controls");
static_assert(MAX_MIXERS <= 64 && MAX_EXPOS <= 64, "RTC mixer capacity");
static_assert(MAX_OUTPUT_CHANNELS <= 32 && NUM_MODULES == 2, "RTC RF capacity");
static_assert(MAX_CURVES <= 32 && MAX_CURVE_POINTS <= 512, "RTC curves");
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
  s.model.throttleReversed = g_model.throttleReversed;
  s.model.extendedLimits = g_model.extendedLimits;
  s.model.jitterFilter = g_model.jitterFilter;
  for (unsigned i = 0; i < MAX_MIXERS; ++i) {
    s.model.mixes[i].destCh = g_model.mixData[i].destCh;
    s.model.mixes[i].mltpx = g_model.mixData[i].mltpx;
    s.model.mixes[i].srcRaw = g_model.mixData[i].srcRaw;
    s.model.mixes[i].weight = g_model.mixData[i].weight;
    s.model.mixes[i].offset = g_model.mixData[i].offset;
    s.model.mixes[i].swtch = g_model.mixData[i].swtch;
    s.model.mixes[i].curveType = g_model.mixData[i].curve.type;
    s.model.mixes[i].curveValue = g_model.mixData[i].curve.value;
  }
  for (unsigned i = 0; i < MAX_EXPOS; ++i) {
    s.model.inputs[i].mode = g_model.expoData[i].mode;
    s.model.inputs[i].chn = g_model.expoData[i].chn;
    s.model.inputs[i].scale = g_model.expoData[i].scale;
    s.model.inputs[i].srcRaw = g_model.expoData[i].srcRaw;
    s.model.inputs[i].weight = g_model.expoData[i].weight;
    s.model.inputs[i].offset = g_model.expoData[i].offset;
    s.model.inputs[i].swtch = g_model.expoData[i].swtch;
    s.model.inputs[i].curveType = g_model.expoData[i].curve.type;
    s.model.inputs[i].curveValue = g_model.expoData[i].curve.value;
  }
  for (unsigned i = 0; i < MAX_OUTPUT_CHANNELS; ++i) {
    s.model.limits[i].min = g_model.limitData[i].min;
    s.model.limits[i].max = g_model.limitData[i].max;
    s.model.limits[i].ppmCenter = g_model.limitData[i].ppmCenter;
    s.model.limits[i].offset = g_model.limitData[i].offset;
    s.model.limits[i].symetrical = g_model.limitData[i].symetrical;
    s.model.limits[i].revert = g_model.limitData[i].revert;
    s.model.limits[i].curve = g_model.limitData[i].curve;
  }
  for (unsigned i = 0; i < MAX_CURVES; ++i) {
    s.model.curves[i].type = g_model.curves[i].type;
    s.model.curves[i].smooth = g_model.curves[i].smooth;
    s.model.curves[i].points = g_model.curves[i].points;
  }
  for (unsigned i = 0; i < NUM_MODULES; ++i) {
    s.model.modules[i].type = g_model.moduleData[i].type;
    s.model.modules[i].channelsStart = g_model.moduleData[i].channelsStart;
    s.model.modules[i].channelsCount = g_model.moduleData[i].channelsCount;
    s.model.modules[i].antennaMode = g_model.moduleData[i].antennaMode;
    s.model.modules[i].telemetryBaudrate = g_model.moduleData[i].crsf.telemetryBaudrate;
    s.model.modules[i].crsfArmingMode = g_model.moduleData[i].crsf.crsfArmingMode;
    s.model.modules[i].crsfArmingTrigger = g_model.moduleData[i].crsf.crsfArmingTrigger;
  }
  for (unsigned i = 0; i < NUM_MODULES; ++i) {
    s.model.modelId[i] = g_model.header.modelId[i];
  }
  for (unsigned i = 0; i < MAX_CURVE_POINTS; ++i) {
    s.model.points[i] = g_model.points[i];
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
  g_model.throttleReversed = s.model.throttleReversed;
  g_model.extendedLimits = s.model.extendedLimits;
  g_model.jitterFilter = s.model.jitterFilter;
  for (unsigned i = 0; i < MAX_MIXERS; ++i) {
    g_model.mixData[i].destCh = s.model.mixes[i].destCh;
    g_model.mixData[i].mltpx = s.model.mixes[i].mltpx;
    g_model.mixData[i].srcRaw = s.model.mixes[i].srcRaw;
    g_model.mixData[i].weight = s.model.mixes[i].weight;
    g_model.mixData[i].offset = s.model.mixes[i].offset;
    g_model.mixData[i].swtch = s.model.mixes[i].swtch;
    g_model.mixData[i].curve.type = s.model.mixes[i].curveType;
    g_model.mixData[i].curve.value = s.model.mixes[i].curveValue;
  }
  for (unsigned i = 0; i < MAX_EXPOS; ++i) {
    g_model.expoData[i].mode = s.model.inputs[i].mode;
    g_model.expoData[i].chn = s.model.inputs[i].chn;
    g_model.expoData[i].scale = s.model.inputs[i].scale;
    g_model.expoData[i].srcRaw = s.model.inputs[i].srcRaw;
    g_model.expoData[i].weight = s.model.inputs[i].weight;
    g_model.expoData[i].offset = s.model.inputs[i].offset;
    g_model.expoData[i].swtch = s.model.inputs[i].swtch;
    g_model.expoData[i].curve.type = s.model.inputs[i].curveType;
    g_model.expoData[i].curve.value = s.model.inputs[i].curveValue;
  }
  for (unsigned i = 0; i < MAX_OUTPUT_CHANNELS; ++i) {
    g_model.limitData[i].min = s.model.limits[i].min;
    g_model.limitData[i].max = s.model.limits[i].max;
    g_model.limitData[i].ppmCenter = s.model.limits[i].ppmCenter;
    g_model.limitData[i].offset = s.model.limits[i].offset;
    g_model.limitData[i].symetrical = s.model.limits[i].symetrical;
    g_model.limitData[i].revert = s.model.limits[i].revert;
    g_model.limitData[i].curve = s.model.limits[i].curve;
  }
  for (unsigned i = 0; i < MAX_CURVES; ++i) {
    g_model.curves[i].type = s.model.curves[i].type;
    g_model.curves[i].smooth = s.model.curves[i].smooth;
    g_model.curves[i].points = s.model.curves[i].points;
  }
  for (unsigned i = 0; i < NUM_MODULES; ++i) {
    g_model.moduleData[i].type = s.model.modules[i].type;
    g_model.moduleData[i].channelsStart = s.model.modules[i].channelsStart;
    g_model.moduleData[i].channelsCount = s.model.modules[i].channelsCount;
    g_model.moduleData[i].antennaMode = s.model.modules[i].antennaMode;
    g_model.moduleData[i].crsf.telemetryBaudrate = s.model.modules[i].telemetryBaudrate;
    g_model.moduleData[i].crsf.crsfArmingMode = s.model.modules[i].crsfArmingMode;
    g_model.moduleData[i].crsf.crsfArmingTrigger = s.model.modules[i].crsfArmingTrigger;
  }
  for (unsigned i = 0; i < NUM_MODULES; ++i) {
    g_model.header.modelId[i] = s.model.modelId[i];
  }
  for (unsigned i = 0; i < MAX_CURVE_POINTS; ++i) {
    g_model.points[i] = s.model.points[i];
  }
  // Rebuild curveEnd[] since after a reboot it starts zeroed and is never
  // otherwise recomputed for an emergency-restored model.
  rebuildCurveCache();
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

