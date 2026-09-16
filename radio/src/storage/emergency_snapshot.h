// LeanTX RTC-only recovery format. No persistent-storage types or annotations.
#pragma once
#include <stdint.h>

// Native scalar layout is intentional: snapshots are local to this firmware.
// Bump the version whenever fields or their interpretation change.
constexpr uint32_t EMERGENCY_SNAPSHOT_MAGIC = 0x45525443;
constexpr uint16_t EMERGENCY_SNAPSHOT_VERSION = 2;
struct EmergencySnapshotHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t payloadLength;
  uint32_t snapshotLength;
  uint32_t buildTag;
  uint32_t crc;
};
struct EmergencyMixSnapshot {
  uint8_t destCh;
  uint8_t mltpx;
  int16_t srcRaw;
  int16_t weight;
  int16_t offset;
  int16_t swtch;
  uint8_t curveType;
  int16_t curveValue;
};
struct EmergencyInputSnapshot {
  uint8_t mode;
  uint8_t chn;
  uint16_t scale;
  int16_t srcRaw;
  int16_t weight;
  int16_t offset;
  int16_t swtch;
  uint8_t curveType;
  int16_t curveValue;
};
struct EmergencyLimitSnapshot {
  int16_t min;
  int16_t max;
  int16_t ppmCenter;
  int16_t offset;
  uint8_t symetrical;
  uint8_t revert;
  int8_t curve;
};
struct EmergencyCurveSnapshot {
  uint8_t type;
  uint8_t smooth;
  int8_t points;
};
struct EmergencyModuleSnapshot {
  uint8_t type;
  uint8_t channelsStart;
  int8_t channelsCount;
  int8_t antennaMode;
  uint8_t telemetryBaudrate;
  uint8_t crsfArmingMode;
  int16_t crsfArmingTrigger;
};
struct EmergencySwitchSnapshot {
  uint8_t type;
  uint8_t start;
  uint8_t group;
  uint8_t state;
};
struct EmergencyRadioSnapshot {
  uint8_t stickMode;
  uint8_t stickInvert;
  uint8_t internalModule;
  uint8_t internalModuleBaudrate;
  uint8_t noJitterFilter;
  int8_t antennaMode;
  int8_t switchesDelay;
  uint8_t backlightMode;
  uint8_t backlightBright;
  uint8_t lightAutoOff;
  uint64_t potsConfig;
  int8_t flexChannels[20];
  int16_t calibration[20][3];
  EmergencySwitchSnapshot switches[20];
  uint8_t contrast;
  uint8_t invertLCD;
  uint8_t stickDeadZone;
  int8_t uartSampleMode;
  int8_t imuMax, imuOffset;
  uint8_t imuInvert;
};
struct EmergencyModelSnapshot {
  uint8_t throttleReversed;
  uint8_t extendedLimits;
  uint8_t jitterFilter;
  uint8_t modelId[2];
  EmergencyMixSnapshot mixes[64];
  EmergencyInputSnapshot inputs[64];
  EmergencyLimitSnapshot limits[32];
  EmergencyCurveSnapshot curves[32];
  int8_t points[512];
  EmergencyModuleSnapshot modules[2];
  EmergencySwitchSnapshot switches[8];
  uint8_t cfsGroupOn;
};
struct EmergencySnapshot {
  EmergencyRadioSnapshot radio;
  EmergencyModelSnapshot model;
};

void captureEmergencySnapshot(EmergencySnapshot &snapshot);
// Conversion only: caller must validate the RTC envelope before restoring.
void restoreEmergencySnapshot(const EmergencySnapshot &snapshot);
