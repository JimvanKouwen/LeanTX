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

#include "gtest/gtest.h"
#include "gtests.h"
#include "pulses/rf_internal.h"
#include "telemetry/telemetry.h"
#include "telemetry/crossfire.h"
#include "crc.h"

#if defined(CROSSFIRE)

uint8_t createCrossfireChannelsFrame(uint8_t moduleIdx, uint8_t * frame, const int16_t * pulses);

class PhysicalControlRfTest : public EdgeTxTest {};

TEST_F(PhysicalControlRfTest, PhysicalMappingReachesCRSFWithOnlyProtocolClamping)
{
  MODEL_RESET();
#if defined(STICK_DEAD_ZONE)
  g_eeGeneral.stickDeadZone = 0;
#endif
  for (int ch = 0; ch < CROSSFIRE_CHANNELS_COUNT; ++ch) {
    g_model.channelMappings[ch].source = physicalStick(0);
  }
  const struct { int16_t physical, mapped; uint16_t encoded; } cases[] = {
    {-1024, -1024, 173}, {-512, -512, 583}, {-1, -1, 992},
    {0, 0, 992}, {1, 1, 992}, {512, 512, 1401}, {1024, 1024, 1811}
  };
  for (const auto& test : cases) {
    anaSetFiltered(inputMappingConvertMode(0), test.physical);
    updateChannelOutputs();
    uint8_t frame[CROSSFIRE_FRAME_MAXLEN] = {};
    createCrossfireChannelsFrame(EXTERNAL_MODULE, frame, channelOutputs);
    for (int ch = 0; ch < CROSSFIRE_CHANNELS_COUNT; ++ch) {
      // Independently unpack the 11-bit wire representation.
      uint16_t encoded = 0;
      for (int bit = 0; bit < 11; ++bit) {
        const int offset = ch * 11 + bit;
        encoded |= ((frame[3 + offset / 8] >> (offset % 8)) & 1) << bit;
      }
      EXPECT_EQ(test.encoded, encoded) << ch << ": " << test.physical;
      EXPECT_EQ(test.mapped, channelOutputs[ch]) << ch;
    }
  }
}

// Spec-defined part of the frame (0x16 RC Channels Packed): sync byte, type,
// 16 x 11-bit channel packing. Expected bytes computed independently, not
// derived from createCrossfireChannelsFrame() itself.
TEST(Crossfire, createCrossfireChannelsFrame)
{
  MODEL_RESET();

  int16_t pulsesStart[CROSSFIRE_CHANNELS_COUNT];
  uint8_t crossfire[CROSSFIRE_FRAME_MAXLEN];

  memset(crossfire, 0, sizeof(crossfire));
  for (int i=0; i<CROSSFIRE_CHANNELS_COUNT; i++) {
    pulsesStart[i] = -1024 + (2048 / CROSSFIRE_CHANNELS_COUNT) * i;
  }

  createCrossfireChannelsFrame(EXTERNAL_MODULE, crossfire, pulsesStart);

  ASSERT_EQ(crossfire[0], MODULE_ADDRESS);
  ASSERT_EQ(crossfire[2], CHANNELS_ID);

  const uint8_t expectedChannelData[22] = {
    0xAD, 0xA0, 0x88, 0x5E, 0xC0, 0x73, 0xA4, 0x56, 0x51, 0x4C, 0x6F,
    0xE0, 0x33, 0x22, 0x2B, 0x27, 0x9A, 0x57, 0xF0, 0x1A, 0x99, 0xD5
  };
  ASSERT_EQ(memcmp(&crossfire[3], expectedChannelData, sizeof(expectedChannelData)), 0);
}

// Status byte after the 0x16 payload is an ExpressLRS extension, not TBS CRSF
// spec (semantics per ExpressLRS's TXModuleEndpoint.cpp / crsf_protocol.h).
// Frame is always 25 bytes (1 ID + 22 channel data + 1 status + 1 CRC);
// bit 0 = commanded armed status (Switch mode only), bit 1 = arming mode is CH5.
TEST(Crossfire, ExpressLRSArmingExtension_CH5Mode)
{
  MODEL_RESET();

  int16_t pulsesStart[CROSSFIRE_CHANNELS_COUNT];
  uint8_t crossfire[CROSSFIRE_FRAME_MAXLEN];

  memset(crossfire, 0, sizeof(crossfire));
  for (int i=0; i<CROSSFIRE_CHANNELS_COUNT; i++) {
    pulsesStart[i] = -1024 + (2048 / CROSSFIRE_CHANNELS_COUNT) * i;
  }

  g_model.moduleData[EXTERNAL_MODULE].crsf.crsfArmingMode = ARMING_MODE_CH5;

  uint8_t len = createCrossfireChannelsFrame(EXTERNAL_MODULE, crossfire, pulsesStart);

  ASSERT_EQ(len, 27);
  ASSERT_EQ(crossfire[0], MODULE_ADDRESS);
  ASSERT_EQ(crossfire[1], 25);
  ASSERT_EQ(crossfire[2], CHANNELS_ID);
  ASSERT_EQ(crossfire[25], 0x02); // bit 1: arming mode CH5

  uint8_t crc = crc8(&crossfire[2], 24);
  ASSERT_EQ(crossfire[26], crc);
}

TEST(Crossfire, ExpressLRSArmingExtension_SwitchMode)
{
  MODEL_RESET();

  int16_t pulsesStart[CROSSFIRE_CHANNELS_COUNT];
  uint8_t crossfire[CROSSFIRE_FRAME_MAXLEN];

  memset(crossfire, 0, sizeof(crossfire));
  for (int i=0; i<CROSSFIRE_CHANNELS_COUNT; i++) {
    pulsesStart[i] = -1024 + (2048 / CROSSFIRE_CHANNELS_COUNT) * i;
  }

  g_model.moduleData[EXTERNAL_MODULE].crsf.crsfArmingMode = ARMING_MODE_SWITCH;
  g_model.moduleData[EXTERNAL_MODULE].crsf.crsfArmingCondition = 0;

  uint8_t len = createCrossfireChannelsFrame(EXTERNAL_MODULE, crossfire, pulsesStart);

  ASSERT_EQ(len, 27);
  ASSERT_EQ(crossfire[0], MODULE_ADDRESS);
  ASSERT_EQ(crossfire[1], 25);
  ASSERT_EQ(crossfire[2], CHANNELS_ID);
  ASSERT_EQ(crossfire[25], 0); // Empty condition -> not armed, bit 1 clear (Switch mode)

  uint8_t crc = crc8(&crossfire[2], 24);
  ASSERT_EQ(crossfire[26], crc);
}

TEST_F(PhysicalControlRfTest, ArmingUsesPhysicalPositionsImmediately)
{
  MODEL_RESET();
  int16_t pulses[CROSSFIRE_CHANNELS_COUNT] = {};
  uint8_t frame[CROSSFIRE_FRAME_MAXLEN] = {};
  auto& md = g_model.moduleData[EXTERNAL_MODULE];
  md.crsf.crsfArmingMode = ARMING_MODE_SWITCH;
  for (auto type : {SWITCH_2POS, SWITCH_3POS}) {
    int sw = findHwSwitch(type);
    if (sw < 0) continue; // Some targets have no switches of this type.
    for (int active = 0; active < 3; ++active) {
      md.crsf.crsfArmingCondition = physicalSwitchCondition(sw, active);
      EXPECT_EQ(type == SWITCH_3POS || active != 1,
                isPhysicalSwitchConditionAvailable(md.crsf.crsfArmingCondition));
      for (int position : {-1, 0, 1}) {
        if (type == SWITCH_2POS && position == 0) continue;
        simuSetSwitch(sw, position);
        ASSERT_EQ(27, createCrossfireChannelsFrame(EXTERNAL_MODULE, frame, pulses));
        EXPECT_EQ(active == position + 1, frame[25]);
        EXPECT_EQ(crc8(frame + 2, 24), frame[26]);
      }
    }
    g_eeGeneral.switchSetType(sw, SWITCH_NONE);
    createCrossfireChannelsFrame(EXTERNAL_MODULE, frame, pulses);
    EXPECT_EQ(0, frame[25]);
  }
  for (int condition : {0, 97, 255}) {
    md.crsf.crsfArmingCondition = condition;
    createCrossfireChannelsFrame(EXTERNAL_MODULE, frame, pulses);
    EXPECT_EQ(0, frame[25]);
  }
  setModuleType(EXTERNAL_MODULE, MODULE_TYPE_CROSSFIRE);
  EXPECT_EQ(ARMING_MODE_CH5, md.crsf.crsfArmingMode);
  EXPECT_EQ(0, md.crsf.crsfArmingCondition);
}

#if defined(FUNCTION_SWITCHES)
TEST_F(PhysicalControlRfTest, FunctionSwitchCannotArm)
{
  MODEL_RESET();
  setModelDefaults();
  int16_t pulses[CROSSFIRE_CHANNELS_COUNT] = {};
  uint8_t frame[CROSSFIRE_FRAME_MAXLEN] = {};
  auto& md = g_model.moduleData[EXTERNAL_MODULE];
  md.crsf.crsfArmingMode = ARMING_MODE_SWITCH;
  for (unsigned sw = 0; sw < switchGetMaxSwitches(); ++sw) {
    if (!switchIsCustomSwitch(sw)) continue;
    g_model.cfsSetType(sw, SWITCH_TOGGLE);
    for (int active : {0, 2}) {
      md.crsf.crsfArmingCondition = physicalSwitchCondition(sw, active);
      ASSERT_FALSE(isPhysicalSwitchConditionAvailable(md.crsf.crsfArmingCondition));
      for (bool state : {false, true}) {
        g_model.cfsSetState(sw, state);
        createCrossfireChannelsFrame(EXTERNAL_MODULE, frame, pulses);
        EXPECT_EQ(0, frame[25]);
      }
    }
    EXPECT_FALSE(isPhysicalSwitchConditionAvailable(physicalSwitchCondition(sw, 1)));
  }
}
#endif

#if defined(CROSSFIRE)
TEST_F(PhysicalControlRfTest, OlderElrsClearsConditionEvenInCH5Mode)
{
  MODEL_RESET();
  uint8_t info[64] = {};
  info[1] = 19; // one-byte (empty) name plus device info
  info[2] = DEVICE_INFO_ID;
  info[4] = MODULE_ADDRESS;
  memcpy(info + 6, "ELRS", 4);
  auto& md = g_model.moduleData[EXTERNAL_MODULE];
  for (int version : {4, 3}) {
    for (int mode : {ARMING_MODE_CH5, ARMING_MODE_SWITCH}) {
      md.crsf.crsfArmingMode = mode;
      md.crsf.crsfArmingCondition = physicalSwitchCondition(0, 2);
      info[15] = version;
      processCrossfireTelemetryFrame(EXTERNAL_MODULE, info, 21);
      EXPECT_EQ(version == 4 ? mode : ARMING_MODE_CH5, md.crsf.crsfArmingMode);
      EXPECT_EQ(version == 4 ? physicalSwitchCondition(0, 2) : 0,
                md.crsf.crsfArmingCondition);
    }
  }
  // A non-ELRS module must not inherit the previous module's ELRS flag.
  memset(info + 6, 0, 4);
  info[15] = 4;
  md.crsf.crsfArmingMode = ARMING_MODE_SWITCH;
  md.crsf.crsfArmingCondition = physicalSwitchCondition(0, 2);
  processCrossfireTelemetryFrame(EXTERNAL_MODULE, info, 21);
  EXPECT_EQ(ARMING_MODE_CH5, md.crsf.crsfArmingMode);
  EXPECT_EQ(0, md.crsf.crsfArmingCondition);
}
#endif

TEST(Crossfire, crc8)
{
  uint8_t frame[] = { 0x00, 0x0C, 0x14, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x01, 0x03, 0x00, 0x00, 0x00, 0xF4 };
  uint8_t crc = crc8(&frame[2], frame[1]-1);
  ASSERT_EQ(frame[frame[1]+1], crc);
}

#if defined(HARDWARE_EXTERNAL_MODULE)
#include "pulses/crossfire.h"

struct crsf_frame_test {
  void* ctx = nullptr;

  uint8_t buffer[TELEMETRY_RX_PACKET_SIZE];
  uint8_t len = 0;

  crsf_frame_test()
  {
    ctx = CrossfireDriver.init(EXTERNAL_MODULE);
    if (!luaInputTelemetryFifo) {
      luaInputTelemetryFifo = new TelemetryQueue();
      assert(luaInputTelemetryFifo != nullptr);
    } else {
      luaInputTelemetryFifo->clear();
    }
  }

  template<unsigned Len>
  void process(uint8_t (&frame)[Len]) {
    CrossfireDriver.processFrame(ctx, frame, Len, buffer, &len);    
  }

  ~crsf_frame_test()
  {
    if (ctx != nullptr) {
      CrossfireDriver.deinit(ctx);
    }
  }
};

static uint8_t incomplete_frame[] = {
    // first frame
    0xEA, 0x14, 0xFF, 0x11, 0xFD, 0x05, 0x00, 0x00, 0x13, 0x01, 0x01,
    0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x5D, 0x98, 0xB4,
    // second frame
    0xEA, 0x21, 0xFF, 0x1E, 0xFD, 0x12, 0x00, 0x00, 0x14, 0x01, 0x01,
    0x4A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x8F, 0xC2, 0x35, 0x3F, 0xFD, 0xC7, 0xD1, 0x3C, 0x4C, 0x01, 0x92,
    0x3F, 0x31,
    // incomplete third frame
    0xEA, 0x1F, 0xFF, 0x1C, 0xFD, 0x10, 0x00,
};

static uint8_t cont_frame[] = {
    0x00, 0x15, 0x01, 0x01, 0x24, 0x00, 0x00, 0x35, 0x7D, 0xF2, 0x40,
    0xE8, 0x03, 0xE8, 0x03, 0xDC, 0x05, 0xDC, 0x05, 0xE1, 0x05, 0xE1,
    0x05, 0x2A, 0xFE, 0x5F,
    //next trailing packet start
    0xEA, 0x0A, 0x0B
};

TEST(Crossfire, frameParser_incompleteFrames)
{
  crsf_frame_test ft;
  if (!ft.ctx) return;

  ft.process(incomplete_frame);
  EXPECT_EQ(ft.len, 7);
  EXPECT_EQ(ft.buffer[0], 0xEA);
  EXPECT_EQ(ft.buffer[1], 0x1F);

  ft.process(cont_frame);
  EXPECT_EQ(ft.len, 3);
  EXPECT_EQ(ft.buffer[0], 0xEA);
  EXPECT_EQ(ft.buffer[1], 0x0A);

  uint8_t* lua_buffer = luaInputTelemetryFifo->buffer();
  EXPECT_EQ(luaInputTelemetryFifo->size(), (size_t)(0x14 + 0x21 + 0x1F));

  unsigned offset = 0;
  EXPECT_EQ(lua_buffer[offset], 0x14);
  offset += 0x14;

  EXPECT_EQ(lua_buffer[offset], 0x21);
  offset += 0x21;

  EXPECT_EQ(lua_buffer[offset], 0x1F);
  EXPECT_EQ(lua_buffer[offset + 0x1F - 1], 0xFE);
}

static uint8_t length_error[] = {
    0x2A, 0xFE, 0x5F, 0x00,
};

static uint8_t length_error2[] = {
    // first frame
    0xEA, 0x09, 0xFF, 0x11, 0xFD, 0x05, 0x00, 0x00, 0x13, 0x01, 0x8C,
    // 2nd incomplete frame
    0xEA, 0xFE, 0x5F, 0x00,
};

static uint8_t length_error3[] = {
    // first frame
    0xEA, 0x09, 0xFF, 0x11, 0xFD, 0x05, 0x00, 0x00, 0x13, 0x01, 0x8C,
    // invalid: invalid frame start
    0x2A, 0x09, 0x5F, 0x00,
    // 2nd valid frame
    0xEA, 0x09, 0xFF, 0x11, 0xFD, 0x05, 0x00, 0x00, 0x13, 0x01, 0x8C,
};

TEST(Crossfire, frameParser_length)
{
  crsf_frame_test ft;
  if (!ft.ctx) return;

  uint8_t* lua_buffer = luaInputTelemetryFifo->buffer();

  // Check that a frame that is too big is rejected even if incomplete
  ft.process(length_error);
  EXPECT_EQ(ft.len, 0);

  // Check that a frame that is too big is rejected if positioned
  // after a complete frame
  ft.process(length_error2);
  EXPECT_EQ(ft.len, 0);

  // the first complete frame should have been processed
  EXPECT_EQ(luaInputTelemetryFifo->size(), (size_t)0x09);

  ft.process(length_error3);
  EXPECT_EQ(ft.len, 0);

  // only the first frame has been processed, as the rest
  // of the input buffer is thrown away due to length error
  EXPECT_EQ(luaInputTelemetryFifo->size(), (size_t)(0x09 + 0x09 + 0x09));

  // check all 3 frames
  unsigned offset = 0;
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(lua_buffer[offset], 0x09);
    EXPECT_EQ(lua_buffer[offset + 0x09 - 1], 0x01);
    offset += 0x09;
  }
}

static uint8_t invalid_frames[] = {
    // first frame
    0xEA, 0x14, 0xFF, 0x11, 0xFD, 0x05, 0x00, 0x00, 0x13, 0x01, 0x01,
    0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x5D, 0x98, 0xB4,
    // random bytes
    0x00, 0x35, 0xA4,
    // second frame
    0xEA, 0x21, 0xFF, 0x1E, 0xFD, 0x12, 0x00, 0x00, 0x14, 0x01, 0x01,
    0x4A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x8F, 0xC2, 0x35, 0x3F, 0xFD, 0xC7, 0xD1, 0x3C, 0x4C, 0x01, 0x92,
    0x3F, 0x31,
    // invalid CRC frame
    0xEA, 0x02, 0x00, 0x01,
    // third frame
    0xEA, 0x1F, 0xFF, 0x1C, 0xFD, 0x10, 0x00,0x00, 0x15, 0x01, 0x01, 0x24, 0x00, 0x00, 0x35, 0x7D, 0xF2, 0x40,
    0xE8, 0x03, 0xE8, 0x03, 0xDC, 0x05, 0xDC, 0x05, 0xE1, 0x05, 0xE1,
    0x05, 0x2A, 0xFE, 0x5F,
};

TEST(Crossfire, frameParser_badFrames)
{
  //check if frameParser correctly skips bad frames (too long, bad CRC) and does't lose following packets 
  crsf_frame_test ft;
  if (!ft.ctx) return;

  ft.process(invalid_frames);
  EXPECT_EQ(ft.len,0);

  uint8_t* lua_buffer = luaInputTelemetryFifo->buffer();
  EXPECT_EQ(luaInputTelemetryFifo->size(), (size_t)(0x14 + 0x21 + 0x1F));

  unsigned offset = 0;
  EXPECT_EQ(lua_buffer[offset], 0x14);
  offset += 0x14;

  EXPECT_EQ(lua_buffer[offset], 0x21);
  offset += 0x21;

  EXPECT_EQ(lua_buffer[offset], 0x1F);
  EXPECT_EQ(lua_buffer[offset + 0x1F - 1], 0xFE);
}

static uint8_t jumboFrame1[]={
  0xEA, 0x3E, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFB
};
static uint8_t jumboFrame2[]={ //jumbo frame 2 
  0x8D, 0xEA, 0x3D, 0xFE, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x57
};

TEST(Crossfire, frameParser_multipleJumboFrames)
{
  crsf_frame_test ft;
  if (!ft.ctx) return;

  ft.process(jumboFrame1);
  EXPECT_EQ(ft.len, 63);
  EXPECT_EQ(ft.buffer[0], 0xEA);
  EXPECT_EQ(ft.buffer[1], 0x3E);

  ft.process(jumboFrame2);
  EXPECT_EQ(ft.len,0);

  uint8_t* lua_buffer = luaInputTelemetryFifo->buffer();
  EXPECT_EQ(luaInputTelemetryFifo->size(), (size_t)(62 + 61));

  unsigned offset = 0;
  EXPECT_EQ(lua_buffer[offset], 0x3E);
  EXPECT_EQ(lua_buffer[offset + 0x3E - 1], 0xFB);

  EXPECT_EQ(lua_buffer[offset], 0x3E);
  offset += 0x3E;

  EXPECT_EQ(lua_buffer[offset], 0x3D);
  EXPECT_EQ(lua_buffer[offset + 0x3D - 1], 0xF0);
}
#endif // HARDWARE_EXTERNAL_MODULE
#endif

#if defined(CROSSFIRE)
#include "telemetry/crsf_device.h"
#include "pulses/crossfire.h"

TEST_F(PhysicalControlRfTest, DeviceTrafficCannotStarveOrReplaceChannels)
{
  MODEL_RESET();
  moduleState[EXTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_CROSSFIRE;
  moduleState[INTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_UNINITIALIZED;
  moduleState[EXTERNAL_MODULE].mode = MODULE_MODE_NORMAL;
  moduleState[EXTERNAL_MODULE].counter = CRSF_FRAME_MODELID_SENT;
  crossfireModuleStatus[EXTERNAL_MODULE].queryCompleted = true;
  CrsfDevice::cancel();
  uint8_t frame[64];
  uint8_t expected[64];
  for (int i = 0; i < CROSSFIRE_CHANNELS_COUNT; ++i) channelOutputs[i] = i * 100 - 800;
  auto channelSize = createCrossfireChannelsFrame(EXTERNAL_MODULE, expected, channelOutputs);
  const uint8_t request[] = {0xEE, 0xEA, 1, 6};
  bool lastDevice = false;
  unsigned channelCount = 0, deviceCount = 0;
  for (int slot = 0; slot < 200; ++slot) {
    if (CrsfDevice::available()) {
      ASSERT_TRUE(CrsfDevice::send(0x2D, request, sizeof(request)));
    }
    EXPECT_FALSE(CrsfDevice::send(0x16, request, sizeof(request)));
    EXPECT_FALSE(CrsfDevice::send(0x17, request, sizeof(request)));
    auto size = setupPulsesCrossfire(EXTERNAL_MODULE, frame, channelOutputs);
    if (frame[2] == CHANNELS_ID) {
      EXPECT_EQ(channelSize, size);
      EXPECT_EQ(0, memcmp(expected, frame, channelSize));
      ++channelCount;
      lastDevice = false;
    } else if (frame[2] == 0x2D) {
      EXPECT_FALSE(lastDevice);
      lastDevice = true;
      ++deviceCount;
    }
  }
  EXPECT_GE(channelCount, 99u);
  EXPECT_GE(deviceCount, 99u);
  CrsfDevice::cancel();
}
#endif

#if defined(CROSSFIRE)
TEST_F(PhysicalControlRfTest, DeviceServiceEnforcesBoundsAndRouting)
{
  moduleState[INTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_NONE;
  moduleState[EXTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_CROSSFIRE;
  CrsfDevice::cancel();
  uint8_t payload[61] = {0xEE, 0xEA, 1, 0};
  EXPECT_FALSE(CrsfDevice::send(0x2D, payload, 61));
  EXPECT_FALSE(CrsfDevice::send(0x2D, payload, SIZE_MAX));
  EXPECT_FALSE(CrsfDevice::send(0x2D, nullptr, 4));
  EXPECT_FALSE(CrsfDevice::send(0x28, payload, 4));
  EXPECT_FALSE(CrsfDevice::send(0x2C, payload, 60));
  EXPECT_TRUE(CrsfDevice::available());
  ASSERT_TRUE(CrsfDevice::send(0x2D, payload, 60));
  EXPECT_FALSE(CrsfDevice::send(0x2D, payload, 4));
  uint8_t frame[64] = {};
  EXPECT_EQ(0u, CrsfDevice::take(INTERNAL_MODULE, frame, sizeof(frame)));
  EXPECT_EQ(0u, CrsfDevice::take(EXTERNAL_MODULE, frame, sizeof(frame) - 1));
  EXPECT_FALSE(CrsfDevice::available());
  EXPECT_EQ(64u, CrsfDevice::take(EXTERNAL_MODULE, frame, sizeof(frame)));
  EXPECT_EQ(0xEE, frame[3]);
  EXPECT_EQ(0xEA, frame[4]);
  EXPECT_EQ(crc8(frame + 2, 61), frame[63]);
  EXPECT_TRUE(CrsfDevice::available());
  moduleState[EXTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_NONE;
  EXPECT_FALSE(CrsfDevice::active());
  EXPECT_FALSE(CrsfDevice::send(0x2D, payload, 4));
}
#endif

TEST_F(PhysicalControlRfTest, RfCapabilityDiscoveryRejectsMalformedFrames)
{
  MODEL_RESET();
  uint8_t info[21] = {};
  info[1] = sizeof(info) - 2;
  info[2] = DEVICE_INFO_ID;
  info[4] = MODULE_ADDRESS;
  memcpy(info + 6, "ELRS", 4);
  info[15] = 4;
  info[16] = 1;
  RfService::receiveDeviceInfo(EXTERNAL_MODULE, info, sizeof(info));
  ASSERT_TRUE(RfService::elrsVersionAtLeast(EXTERNAL_MODULE, 4, 1));
  EXPECT_FALSE(RfService::elrsVersionAtLeast(EXTERNAL_MODULE, 4, 2));
  auto snapshot = RfService::capabilities(EXTERNAL_MODULE);
  snapshot.major = 0;
  EXPECT_EQ(0, snapshot.major);
  EXPECT_EQ(4, RfService::capabilities(EXTERNAL_MODULE).major);
  for (size_t len = 0; len < sizeof(info); ++len) {
    info[15] = 3;
    RfService::receiveDeviceInfo(EXTERNAL_MODULE, info, len);
    EXPECT_EQ(4, RfService::capabilities(EXTERNAL_MODULE).major);
  }
  RfService::receiveDeviceInfo(EXTERNAL_MODULE, nullptr, sizeof(info));
  RfService::receiveDeviceInfo(NUM_MODULES, info, sizeof(info));
  info[5] = 'X'; // name now consumes the first byte of the fixed info block
  RfService::receiveDeviceInfo(EXTERNAL_MODULE, info, sizeof(info));
  EXPECT_EQ(4, RfService::capabilities(EXTERNAL_MODULE).major);
  EXPECT_FALSE(RfService::setModulePower(-1, true));
  EXPECT_FALSE(RfService::setModulePower(NUM_MODULES, true));
  EXPECT_FALSE(RfService::setBootPin(-1, true));
  EXPECT_FALSE(RfService::setBootPin(NUM_MODULES, true));
  EXPECT_FALSE(RfService::usesTxHardware(NUM_MODULES, info));
  EXPECT_FALSE(RfService::restartAsync(NUM_MODULES, 1));
  RfService::setMode(NUM_MODULES, MODULE_MODE_BIND);
  EXPECT_EQ(MODULE_MODE_NORMAL, RfService::mode(NUM_MODULES));
  EXPECT_FALSE(RfService::active(NUM_MODULES));
  EXPECT_FALSE(RfService::capabilities(NUM_MODULES).queryCompleted);
}

#if defined(CROSSFIRE)
#include "mixer_scheduler.h"
#include <thread>
#include <atomic>

TEST_F(PhysicalControlRfTest, SilentDiscoveryAndResetRequestsPreserveCadence)
{
  MODEL_RESET();
  moduleState[EXTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_CROSSFIRE;
  moduleState[EXTERNAL_MODULE].mode = MODULE_MODE_NORMAL;
  crossfireModuleStatus[EXTERNAL_MODULE] = {};
  int16_t channels[16] = {};
  uint8_t frame[64];
  int gap = 0, channelsSent = 0, pings = 0;
  for (int slot = 0; slot < 1000; ++slot) {
    if (slot % 17 == 0) RfService::requestModelId(EXTERNAL_MODULE);
    setupPulsesCrossfire(EXTERNAL_MODULE, frame, channels);
    if (frame[2] == CHANNELS_ID) { gap = 0; ++channelsSent; }
    else { EXPECT_LE(++gap, 1); if (frame[2] == PING_DEVICES_ID) ++pings; }
  }
  EXPECT_GE(channelsSent, 500);
  EXPECT_GT(pings, 0);
}

TEST_F(PhysicalControlRfTest, SyncPeriodsAlwaysBounded)
{
  ModuleSyncStatus sync;
  EXPECT_FALSE(sync.isValid());
  for (unsigned rate = 1; rate <= 65535; ++rate) {
    for (int lag : {-32768, 0, 32767}) {
      sync.update(rate, lag);
      for (int n = 0; n < 3; ++n) {
        auto period = sync.getAdjustedRefreshRate();
        EXPECT_GE(period, MIN_REFRESH_RATE);
        EXPECT_LE(period, MAX_REFRESH_RATE);
      }
    }
  }
}

TEST_F(PhysicalControlRfTest, ConcurrentDeviceMailboxIsNeverTorn)
{
  moduleState[INTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_NONE;
  moduleState[EXTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_CROSSFIRE;
  CrsfDevice::cancel();
  std::atomic<bool> done{false};
  std::thread producer([&] {
    uint8_t payload[60] = {MODULE_ADDRESS, RADIO_ADDRESS};
    for (unsigned n = 0; n < 20000; ++n) {
      memset(payload + 2, n & 255, 58);
      CrsfDevice::send(0x2D, payload, sizeof(payload));
      if (n % 3 == 0) CrsfDevice::cancel();
    }
    done.store(true);
  });
  std::thread canceller([&] {
    while (!done.load()) { CrsfDevice::cancel(); std::this_thread::yield(); }
  });
  do {
    uint8_t frame[64];
    auto size = CrsfDevice::take(EXTERNAL_MODULE, frame, sizeof(frame));
    if (size) {
      EXPECT_EQ(64u, size);
      EXPECT_EQ(crc8(frame + 2, 61), frame[63]);
      for (int i = 6; i < 63; ++i) EXPECT_EQ(frame[5], frame[i]);
    }
  } while (!done.load());
  producer.join();
  canceller.join();
  CrsfDevice::cancel();
  EXPECT_TRUE(CrsfDevice::available());
}

#if defined(HARDWARE_EXTERNAL_MODULE)
TEST(Crossfire, MalformedPayloadsAndEveryPartialSplit)
{
  crsf_frame_test ft;
  ASSERT_NE(nullptr, ft.ctx);
  // Every type at every legal length must be safe, including short GPS/time,
  // sync, device-info and array frames. Check with ASan/UBSan.
  for (unsigned type = 0; type < 256; ++type) {
    for (unsigned length = 4; length <= 64; ++length) {
      auto frame = new uint8_t[length]();
      frame[0] = RADIO_ADDRESS;
      frame[1] = length - 2;
      frame[2] = type;
      frame[length - 1] = crc8(frame + 2, length - 3);
      CrossfireDriver.processFrame(ft.ctx, frame, length, ft.buffer, &ft.len);
      EXPECT_EQ(0, ft.len);
      delete[] frame;
    }
  }
  for (unsigned split = 1; split < 64; ++split) {
    uint8_t frame[64] = {RADIO_ADDRESS, 62, 0x7f};
    frame[63] = crc8(frame + 2, 61);
    CrossfireDriver.processFrame(ft.ctx, frame, split, ft.buffer, &ft.len);
    EXPECT_EQ(split, ft.len);
    CrossfireDriver.processFrame(ft.ctx, frame + split, 64 - split, ft.buffer, &ft.len);
    EXPECT_EQ(0, ft.len);
  }
}
#endif
#endif

#if defined(CROSSFIRE) && defined(HARDWARE_EXTERNAL_MODULE)
TEST(Crossfire, ModuleResetResendsCurrentModelId)
{
  MODEL_RESET();
  crsf_frame_test ft;
  ASSERT_NE(nullptr, ft.ctx);
  auto savedTime = g_tmr10ms;
  auto savedTelemetryState = telemetryState;
  telemetryState = TELEMETRY_OK;
  moduleState[EXTERNAL_MODULE].mode = MODULE_MODE_NORMAL;
  moduleState[EXTERNAL_MODULE].counter = CRSF_FRAME_MODELID_SENT;
  crossfireModuleStatus[EXTERNAL_MODULE].queryCompleted = true;
  g_model.header.modelId[EXTERNAL_MODULE] = 37;
  int16_t channels[16] = {};
  uint8_t tx[64];
  uint8_t alive[] = {RADIO_ADDRESS, 2, 0x7f, 0};
  alive[3] = crc8(alive + 2, 1);
  for (int reset = 0; reset < 3; ++reset) {
    g_tmr10ms += 60;
    for (int i = 0; i < 2; ++i) setupPulsesCrossfire(EXTERNAL_MODULE, tx, channels);
    ft.process(alive);
    bool resent = false;
    for (int i = 0; i < 2; ++i) {
      auto length = setupPulsesCrossfire(EXTERNAL_MODULE, tx, channels);
      if (tx[2] == COMMAND_ID) {
        EXPECT_EQ(10u, length);
        EXPECT_EQ(COMMAND_MODEL_SELECT_ID, tx[6]);
        EXPECT_EQ(37, tx[7]);
        EXPECT_EQ(crc8_BA(tx + 2, 6), tx[8]);
        EXPECT_EQ(crc8(tx + 2, 7), tx[9]);
        resent = true;
      }
    }
    EXPECT_TRUE(resent);
  }
  g_tmr10ms = savedTime;
  telemetryState = savedTelemetryState;
}
#endif

#if defined(CROSSFIRE)
TEST_F(PhysicalControlRfTest, BindWorksWithoutCapabilityResponse)
{
  MODEL_RESET();
  moduleState[EXTERNAL_MODULE].counter = CRSF_FRAME_MODELID_SENT;
  moduleState[EXTERNAL_MODULE].mode = MODULE_MODE_BIND;
  crossfireModuleStatus[EXTERNAL_MODULE] = {};
  int16_t channels[16] = {};
  uint8_t frame[64];
  bool bound = false;
  for (int slot = 0; slot < 4; ++slot) {
    auto length = setupPulsesCrossfire(EXTERNAL_MODULE, frame, channels);
    if (frame[2] == COMMAND_ID && frame[6] == SUBCOMMAND_CRSF_BIND) {
      EXPECT_EQ(9u, length);
      EXPECT_EQ(crc8_BA(frame + 2, 5), frame[7]);
      EXPECT_EQ(crc8(frame + 2, 6), frame[8]);
      bound = true;
    }
  }
  EXPECT_TRUE(bound);
  EXPECT_EQ(MODULE_MODE_NORMAL, moduleState[EXTERNAL_MODULE].mode);
}
#endif

#if defined(CROSSFIRE)
TEST_F(PhysicalControlRfTest, FloodWithChangingPeriodsAndBusyUartHasBoundedGap)
{
  MODEL_RESET();
  moduleState[INTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_NONE;
  moduleState[EXTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_CROSSFIRE;
  moduleState[EXTERNAL_MODULE].mode = MODULE_MODE_NORMAL;
  moduleState[EXTERNAL_MODULE].counter = CRSF_FRAME_MODELID_SENT;
  crossfireModuleStatus[EXTERNAL_MODULE].queryCompleted = true;
  CrsfDevice::cancel();
  uint8_t payload[60] = {MODULE_ADDRESS, RADIO_ADDRESS};
  uint8_t frame[64];
  int16_t channels[16] = {};
  uint64_t now = 0, busyUntil = 0, lastChannel = 0;
  bool sawChannel = false;
  unsigned sent = 0;
  // Worst wire speed, including a second module controlling the global tick.
  for (unsigned slot = 0; slot < 20000; ++slot) {
    now += slot % 3 == 0 ? MAX_REFRESH_RATE : MIN_REFRESH_RATE;
    CrsfDevice::send(0x2D, payload, sizeof(payload));
    if (now < busyUntil) continue; // same completion gate as RfService
    auto size = setupPulsesCrossfire(EXTERNAL_MODULE, frame, channels);
    busyUntil = now + (size * 10000000u + 115199) / 115200;
    if (frame[2] == CHANNELS_ID) {
      if (sawChannel) EXPECT_LE(now - lastChannel, 112000u);
      lastChannel = now;
      sawChannel = true;
      ++sent;
    }
  }
  EXPECT_GT(sent, 1000u);
  CrsfDevice::cancel();
}
#endif

#if defined(CROSSFIRE) && defined(HARDWARE_EXTERNAL_MODULE)
TEST(Crossfire, ExpiredPartialFrameCannotConsumeNewReply)
{
  crsf_frame_test ft;
  ASSERT_NE(nullptr, ft.ctx);
  auto savedTime = g_tmr10ms;
  uint8_t partial[] = {RADIO_ADDRESS, 62, 0x7f};
  ft.process(partial);
  ASSERT_EQ(3, ft.len);
  g_tmr10ms += 11;
  uint8_t reply[] = {RADIO_ADDRESS, 2, 0x7f, 0};
  reply[3] = crc8(reply + 2, 1);
  ft.process(reply);
  EXPECT_EQ(0, ft.len);
  EXPECT_EQ(2u, luaInputTelemetryFifo->size());
  g_tmr10ms = savedTime;
}
#endif

#if defined(CROSSFIRE)
TEST(Crossfire, MalformedBaudrateUsesSupportedDefault)
{
  for (unsigned stored = 0; stored <= 255; ++stored) {
    auto index = crossfireBaudrateIndex(stored, CROSSFIRE_MAX_EXTERNAL_BAUDRATE);
    EXPECT_LT(index, DIM(CROSSFIRE_BAUDRATES));
    EXPECT_LE(index, CROSSFIRE_MAX_EXTERNAL_BAUDRATE);
    if (stored >= DIM(CROSSFIRE_BAUDRATES)) EXPECT_EQ(CROSSFIRE_STORE_TO_INDEX(0), index);
  }
}
#endif

#if defined(CROSSFIRE) && defined(HARDWARE_EXTERNAL_MODULE)
TEST(Crossfire, EveryInitializationSelectsModelBeforeChannels)
{
  MODEL_RESET();
  for (int id : {0, 37, 63}) {
    moduleState[EXTERNAL_MODULE].counter = CRSF_FRAME_MODELID_SENT;
    g_model.header.modelId[EXTERNAL_MODULE] = id;
    crsf_frame_test ft;
    ASSERT_NE(nullptr, ft.ctx);
    int16_t channels[16] = {};
    uint8_t tx[64];
    EXPECT_EQ(10u, setupPulsesCrossfire(EXTERNAL_MODULE, tx, channels));
    EXPECT_EQ(COMMAND_ID, tx[2]);
    EXPECT_EQ(COMMAND_MODEL_SELECT_ID, tx[6]);
    EXPECT_EQ(id, tx[7]);
    EXPECT_EQ(27u, setupPulsesCrossfire(EXTERNAL_MODULE, tx, channels));
    EXPECT_EQ(CHANNELS_ID, tx[2]);
  }
}
#endif

#if defined(CROSSFIRE)
uint8_t createCrossfireBindFrame(uint8_t moduleIdx, uint8_t* frame);
TEST(Crossfire, BindDestinationUsesOnlyItsOwnLiveReceiver)
{
  auto savedTime = g_tmr10ms;
  auto savedValid = telemetryData.telemetryValid;
  telemetryData.telemetryValid = 0;
  uint8_t link[14] = {RADIO_ADDRESS, 12, LINK_ID, 0, 0, 100};
  processCrossfireTelemetryFrame(INTERNAL_MODULE, link, sizeof(link));
  uint8_t frame[64];
  createCrossfireBindFrame(EXTERNAL_MODULE, frame);
  EXPECT_EQ(MODULE_ADDRESS, frame[3]);
  createCrossfireBindFrame(INTERNAL_MODULE, frame);
  EXPECT_EQ(RECEIVER_ADDRESS, frame[3]);
  g_tmr10ms += TELEMETRY_TIMEOUT10ms;
  createCrossfireBindFrame(INTERNAL_MODULE, frame);
  EXPECT_EQ(MODULE_ADDRESS, frame[3]);
  g_tmr10ms = savedTime;
  telemetryData.telemetryValid = savedValid;
}
#endif
