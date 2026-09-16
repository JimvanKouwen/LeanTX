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

#include "gtests.h"

#if defined(RTC_BACKUP_RAM)
#include "storage/rtc_backup.h"
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"
#include "telemetry/crossfire.h"

uint8_t createCrossfireChannelsFrame(uint8_t moduleIdx, uint8_t *frame, int16_t *pulses);
namespace {
void prepareBackup()
{
  memset(&g_eeGeneral, 0, sizeof(g_eeGeneral));
  memset(&g_model, 0, sizeof(g_model));
  g_eeGeneral.stickMode = 2;
  g_eeGeneral.calib[0].mid = 1024;
  g_eeGeneral.calib[0].spanNeg = 900;
  g_eeGeneral.calib[0].spanPos = 950;
  g_eeGeneral.internalModule = MODULE_TYPE_CROSSFIRE;
  g_eeGeneral.internalModuleBaudrate = 2;
  g_eeGeneral.noJitterFilter = 1;
  g_model.moduleData[0].type = MODULE_TYPE_CROSSFIRE;
  g_model.moduleData[0].channelsStart = 2;
  g_model.moduleData[0].channelsCount = 8;
  g_model.moduleData[0].crsf.telemetryBaudrate = 3;
  g_model.moduleData[0].crsf.crsfArmingMode = 1;
  g_model.moduleData[0].crsf.crsfArmingTrigger = -3;
  g_model.header.modelId[0] = 42;
  g_model.mixData[0].srcRaw = MIXSRC_FIRST_STICK;
  g_model.mixData[0].weight = 87;

  g_model.limitData[0].offset = -120;
  g_model.curves[0].points = 4;
  g_model.points[0] = -100;
  g_model.throttleReversed = 1;
  g_model.jitterFilter = 2;
#if defined(FUNCTION_SWITCHES)
  g_model.customSwitches[0].state = 1;
  g_model.customSwitches[0].group = 2;
  g_model.cfsGroupOn = 1;
#endif
  rambackupWrite();
}

// Independent CRC implementation allows testing malformed streams with valid CRC.
void updateCRC()
{
  uint32_t crc = 0xffffffff;
  auto add = [&crc](uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i)
      crc = crc & 1 ? (crc >> 1) ^ 0xedb88320 : crc >> 1;
  };
  const auto *bytes = reinterpret_cast<const uint8_t *>(&ramBackup->header);
  for (unsigned i = offsetof(EmergencySnapshotHeader, version);
       i < offsetof(EmergencySnapshotHeader, crc); ++i) add(bytes[i]);
  for (unsigned i = 0; i < ramBackup->header.payloadLength; ++i)
    add(ramBackup->data[i]);
  ramBackup->header.crc = ~crc;
}

void expectRejected()
{
  g_model.mixData[0].weight = 51;
  g_eeGeneral.stickMode = 1;
  EXPECT_FALSE(rambackupRestore());
  EXPECT_EQ(51, g_model.mixData[0].weight);
  EXPECT_EQ(1, g_eeGeneral.stickMode);
}
}

TEST(EmergencySnapshot, ControlAndRFRecovery)
{
  prepareBackup();
  ASSERT_EQ(EMERGENCY_SNAPSHOT_MAGIC, ramBackup->header.magic);
  EmergencySnapshot expected, actual;
  captureEmergencySnapshot(expected);
  memset(&g_eeGeneral, 0, sizeof(g_eeGeneral));
  memset(&g_model, 0, sizeof(g_model));
  ASSERT_TRUE(rambackupRestore());
  captureEmergencySnapshot(actual);
  EXPECT_EQ(0, memcmp(&expected, &actual, sizeof(expected)));
  EXPECT_EQ(MODULE_TYPE_CROSSFIRE, g_model.moduleData[0].type);
  EXPECT_EQ(3, g_model.moduleData[0].crsf.telemetryBaudrate);
  EXPECT_EQ(-3, g_model.moduleData[0].crsf.crsfArmingTrigger);
  EXPECT_EQ(42, g_model.header.modelId[0]);
  EXPECT_EQ(87, g_model.mixData[0].weight);
  EXPECT_EQ(-120, g_model.limitData[0].offset);
  printf("Emergency snapshot: raw=%zu compressed=%u remaining=%zu\n",
         sizeof(expected), ramBackup->header.payloadLength,
         sizeof(ramBackup->data) - ramBackup->header.payloadLength);
}
TEST(EmergencySnapshot, BadMagic) {
  prepareBackup(); ramBackup->header.magic ^= 1; expectRejected();
}
TEST(EmergencySnapshot, UnsupportedVersion) {
  prepareBackup(); ++ramBackup->header.version; updateCRC(); expectRejected();
}
TEST(EmergencySnapshot, DifferentBuild) {
  prepareBackup(); ++ramBackup->header.buildTag; updateCRC(); expectRejected();
}
TEST(EmergencySnapshot, BadCRC) {
  prepareBackup(); ramBackup->header.crc ^= 1; expectRejected();
}
TEST(EmergencySnapshot, CorruptPayload) {
  prepareBackup(); ramBackup->data[0] ^= 1; expectRejected();
}
TEST(EmergencySnapshot, InvalidCompressedToken) {
  prepareBackup(); ramBackup->data[0] = 0; updateCRC(); expectRejected();
}
TEST(EmergencySnapshot, TruncatedCompressedPayload) {
  prepareBackup(); --ramBackup->header.payloadLength; updateCRC(); expectRejected();
}
TEST(EmergencySnapshot, IncompleteLiteralRun) {
  prepareBackup();
  ramBackup->header.payloadLength = 2;
  ramBackup->data[0] = 63; ramBackup->data[1] = 1;
  updateCRC(); expectRejected();
}
TEST(EmergencySnapshot, OversizedPayload) {
  prepareBackup(); ramBackup->header.payloadLength = sizeof(ramBackup->data) + 1;
  expectRejected();
}
TEST(EmergencySnapshot, EmptyPayload) {
  prepareBackup(); ramBackup->header.payloadLength = 0; expectRejected();
}
TEST(EmergencySnapshot, WrongDecodedSize) {
  prepareBackup(); ++ramBackup->header.snapshotLength; updateCRC(); expectRejected();
}
TEST(EmergencySnapshot, UnsupportedRFModule) {
  prepareBackup(); g_model.moduleData[0].type = 63;
  rambackupWrite(); expectRejected();
}
TEST(EmergencySnapshot, InvalidRFChannelRange) {
  prepareBackup(); g_model.moduleData[0].channelsStart = MAX_OUTPUT_CHANNELS;
  rambackupWrite(); expectRejected();
}
TEST(EmergencySnapshot, SafeBootFallback) {
  prepareBackup(); ramBackup->header.magic = 0;
  ASSERT_FALSE(rambackupRestoreOrReset());
  EXPECT_EQ(MODULE_TYPE_NONE, g_eeGeneral.internalModule);
  for (const auto &module : g_model.moduleData)
    EXPECT_EQ(MODULE_TYPE_NONE, module.type);
  EXPECT_EQ(0, g_model.mixData[0].srcRaw);
  prepareBackup();
  EXPECT_TRUE(rambackupRestoreOrReset());
  EXPECT_EQ(MODULE_TYPE_CROSSFIRE, g_model.moduleData[0].type);
}
TEST(EmergencySnapshot, DenseControlConfigurationFits) {
  prepareBackup();
  // Exercise a dense configuration rather than only sparse default models.
  uint32_t random = 7;
  auto fill = [&random](void *ptr, size_t size) {
    auto *bytes = static_cast<uint8_t *>(ptr);
    while (size--) {
      random = random * 1664525 + 1013904223;
      *bytes++ = (random >> 24) | 1;
    }
  };
  fill(g_model.mixData, sizeof(g_model.mixData));
  fill(g_model.limitData, sizeof(g_model.limitData));
  fill(g_model.points, sizeof(g_model.points));
  rambackupWrite();
  EXPECT_EQ(EMERGENCY_SNAPSHOT_MAGIC, ramBackup->header.magic);
  EXPECT_LE(ramBackup->header.payloadLength, sizeof(ramBackup->data));
  EXPECT_TRUE(rambackupRestore());
  printf("Dense snapshot: raw=%zu compressed=%u remaining=%zu\n",
         sizeof(EmergencySnapshot), ramBackup->header.payloadLength,
         sizeof(ramBackup->data) - ramBackup->header.payloadLength);
}
TEST(EmergencySnapshot, OversizedDecodedPayload) {
  prepareBackup();
  // Each 0x7f expands to 63 zero bytes, exceeding the snapshot buffer.
  memset(ramBackup->data, 0x7f, sizeof(ramBackup->data));
  ramBackup->header.payloadLength = sizeof(EmergencySnapshot) / 63 + 1;
  updateCRC(); expectRejected();
}
class EmergencyControlTest : public EdgeTxTest {};
TEST_F(EmergencyControlTest, MixerAndCRSFFrameAfterRecovery)
{
  const auto throttle = inputMappingGetThrottle();
  g_eeGeneral.stickMode = 1;
  g_model.throttleReversed = 1;
  g_model.moduleData[EXTERNAL_MODULE].type = MODULE_TYPE_CROSSFIRE;
  g_model.moduleData[EXTERNAL_MODULE].crsf.crsfArmingMode = ARMING_MODE_CH5;
  anaSetFiltered(inputMappingConvertMode(throttle), -1024);
  evalMixes();
  ASSERT_EQ(1024, channelOutputs[throttle]);
  int16_t expected[MAX_OUTPUT_CHANNELS];
  memcpy(expected, channelOutputs, sizeof(expected));
  uint8_t frameBefore[CROSSFIRE_FRAME_MAXLEN]{};
  auto length = createCrossfireChannelsFrame(EXTERNAL_MODULE, frameBefore, expected);
  rambackupWrite();
  memset(&g_eeGeneral, 0, sizeof(g_eeGeneral));
  memset(&g_model, 0, sizeof(g_model));
  MIXER_RESET();
  ASSERT_TRUE(rambackupRestoreOrReset());
  anaSetFiltered(inputMappingConvertMode(throttle), -1024);
  evalMixes();
  ASSERT_EQ(1024, channelOutputs[throttle]);
  EXPECT_EQ(0, memcmp(expected, channelOutputs, sizeof(expected)));
  int16_t restored[MAX_OUTPUT_CHANNELS];
  memcpy(restored, channelOutputs, sizeof(restored));
  uint8_t frameAfter[CROSSFIRE_FRAME_MAXLEN]{};
  ASSERT_EQ(length, createCrossfireChannelsFrame(EXTERNAL_MODULE, frameAfter, restored));
  EXPECT_EQ(0, memcmp(frameBefore, frameAfter, length));
  EXPECT_EQ(2, frameAfter[25]); // ELRS CH5 arming mode preserved.
}
#if defined(FUNCTION_SWITCHES)
TEST(EmergencySnapshot, PhysicalFunctionSwitches)
{
  prepareBackup();
  g_model.customSwitches[0].state = 0;
  g_model.customSwitches[0].group = 0;
  ASSERT_TRUE(rambackupRestore());
  EXPECT_EQ(1, g_model.customSwitches[0].state);
  EXPECT_EQ(2, g_model.customSwitches[0].group);
  EXPECT_EQ(1, g_model.cfsGroupOn);
}
#endif
#if MAX_FLEX_SWITCHES > 0
TEST(EmergencySnapshot, FlexSwitchMapping)
{
  prepareBackup();
  g_eeGeneral.potsConfig = FLEX_SWITCH;
  switchConfigFlex_raw(0, 0);
  ASSERT_EQ(0, switchGetFlexConfig_raw(0));
  rambackupWrite();
  switchConfigFlex_raw(0, -1);
  ASSERT_TRUE(rambackupRestore());
  EXPECT_EQ(0, switchGetFlexConfig_raw(0));
}
#endif
#endif
