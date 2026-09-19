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

#include <math.h>
#include "gtests.h"
#include "pulses/rf_internal.h"
#include "location.h"

#if defined(LUA)

#include "edgetx.h"
#include "lua/lua_states.h"
#include "lua/lua_event.h"

#include <filesystem>
#include <fstream>

#define MIXSRC_THR     (MIXSRC_FIRST_STICK + inputMappingGetThrottle())

::testing::AssertionResult __luaExecStr(const char * str)
{
  extern lua_State * lsScripts;
  if (!lsScripts) { luaInitMainState(); luaInit(); }
  if (!lsScripts) return ::testing::AssertionFailure() << "No Lua state!";
  if (luaL_dostring(lsScripts, str)) {
    return ::testing::AssertionFailure() << "lua error: " << lua_tostring(lsScripts, -1);
  }
  return ::testing::AssertionSuccess();
}

#define luaExecStr(test)  EXPECT_TRUE(__luaExecStr(test))

TEST(Lua, TelemetrySourceCompatibility)
{
  MODEL_RESET();
  luaExecStr("assert(type(getValue) == 'function')");
  auto &sensor = g_model.telemetrySensors[0];
  sensor.unit = UNIT_VOLTS;
  sensor.prec = 2;
  telemetryItems[0].setValue(sensor, 1234, UNIT_VOLTS, 2);
  telemetryStreaming = 100;
  for (int variant = 0; variant < 3; ++variant) {
    const int source = MIXSRC_FIRST_TELEM + variant;
    const auto script = std::string("assert(math.abs(getValue(") +
        std::to_string(source) + ") - 12.34) < 0.001)";
    luaExecStr(script.c_str());
  }
  telemetryStreaming = 0;
  telemetryItems[0].clear();
}

TEST(Lua, ConstantSourcesRemoved)
{
  MODEL_RESET();
  luaExecStr("assert(MIXSRC_MIN == nil and MIXSRC_MAX == nil)");
  luaExecStr("assert(getFieldInfo('min') == nil and getFieldInfo('max') == nil)");
  luaExecStr("assert(getSourceIndex('MIN') == nil and getSourceIndex('MAX') == nil)");
  luaExecStr("assert(getSourceValue('min') == nil and getSourceValue('max') == nil)");
  luaExecStr("for id, name in sources() do "
             "local f = getFieldInfo(id); "
             "assert(f == nil or (f.name ~= 'min' and f.name ~= 'max')) end");
}

TEST(Lua, MixerOutputSourcesRemoved)
{
  luaExecStr("assert(getFieldInfo('lua1a') == nil)");
  luaExecStr("assert(getFieldInfo('LUA1a') == nil)");
  luaExecStr("assert(getFieldInfo('lua1') == nil)");
  luaExecStr("assert(getFieldInfo('timer1') ~= nil)");
  luaExecStr("assert(getFieldInfo('ch1') == nil)");
  luaExecStr("assert(getFieldInfo('ch32') == nil)");
  luaExecStr("assert(MIXSRC_CH1 == nil)");
  channelOutputs[0] = 777;
  luaExecStr("assert(getOutputValue(0) == 777)");
  luaExecStr("assert(type(lcd.drawText) == 'function')");
  luaExecStr("assert(type(playFile) == 'function' and type(io.open) == 'function')");
  luaExecStr("assert(type(crossfireTelemetryPush) == 'function')");
#if defined(COLORLCD)
  luaExecStr("assert(VALUE == 0 and SOURCE == 1)");
#endif
}

#if defined(PCBTARANIS)
class LuaApplications : public testing::Test
{
 protected:
  std::filesystem::path root;
  void SetUp() override
  {
    char path[] = "/tmp/leantx-lua-apps-XXXXXX";
    root = mkdtemp(path);
    std::filesystem::create_directories(root / "SCRIPTS/TOOLS");
    simuFatfsSetPaths(root.c_str(), nullptr);
    luaClose();
    luaState = 0;
    MODEL_RESET();
    luaInitMainState();
  }
  void TearDown() override
  {
    luaClose();
    luaState = 0;
    luaScriptsCount = 0;
    MODEL_RESET();
    simuFatfsSetPaths(TESTS_PATH, nullptr);
    std::filesystem::remove_all(root);
  }
};

TEST_F(LuaApplications, StandaloneToolsStillRun)
{
  LUA_LOAD_MODEL_SCRIPTS();
  luaTask(false);
  ASSERT_EQ(INTERPRETER_START_RUNNING, luaState);
  ASSERT_EQ(0, luaScriptsCount);

  std::ofstream(root / "SCRIPTS/TOOLS/test.lua") <<
      "return {init=function() toolInitialized=true end, "
      "run=function(event) toolEvent=event; return 0 end}";
  luaExec("/SCRIPTS/TOOLS/test.lua");
  ASSERT_EQ(1, luaScriptsCount);
  EXPECT_EQ(SCRIPT_OK, scriptInternalData[0].state);
  luaTask(true);
  luaExecStr("assert(toolInitialized and type(toolEvent) == 'number')");
  luaPushEvent(EVT_KEY_LONG(KEY_EXIT));
  luaTask(true);
  EXPECT_EQ(INTERPRETER_RELOAD_PERMANENT_SCRIPTS, luaState);
  luaTask(false);
  EXPECT_EQ(0, luaScriptsCount);

}
#endif

TEST(Lua, RemovedVariableApis)
{
  luaExecStr("assert(model.getGlobalVariable == nil and model.setGlobalVariable == nil)");
  luaExecStr("assert(model.getGlobalVariableDetails == nil and model.setGlobalVariableDetails == nil)");
  luaExecStr("assert(getFlightMode == nil)");
  luaExecStr("assert(model.getFlightMode == nil and model.setFlightMode == nil)");
  luaExecStr("assert(model.deleteFlightModes == nil)");
}

TEST(Lua, testSetModelInfo)
{
  luaExecStr("info = model.getInfo()");
  // luaExecStr("print('model name: '..info.name..' id: '..info.id)");
  luaExecStr("info.name = 'modelA'");
  luaExecStr("model.setInfo(info)");
  // luaExecStr("print('model name: '..info.name..' id: '..info.id)");
  EXPECT_STRNEQ("modelA", g_model.header.name);

  luaExecStr("info.name = 'Model 1'");
  luaExecStr("model.setInfo(info)");
  // luaExecStr("print('model name: '..info.name..' id: '..info.id)");
  EXPECT_STRNEQ("Model 1", g_model.header.name);
}

TEST(Lua, testPanicProtection)
{
  bool passed = false;
  PROTECT_LUA() {
    PROTECT_LUA() {
      // simulate panic
      longjmp(global_lj->b, 1);
    }
    else {
      // we should come here
      passed = true;
    }
    UNPROTECT_LUA();
  }
  else {
    // and not here
    // TRACE("testLuaProtection: test 1 FAILED");
    FAIL() << "Failed test 1";
  }
  UNPROTECT_LUA()

  EXPECT_EQ(passed, true);

  passed = false;

  PROTECT_LUA() {
    PROTECT_LUA() {
      int a = 5;
      UNUSED(a);
    }
    else {
      // we should not come here
      // TRACE("testLuaProtection: test 2 FAILED");
      FAIL() << "Failed test 2";
    }
    UNPROTECT_LUA()
    // simulate panic
    longjmp(global_lj->b, 1);
  }
  else {
    // we should come here
    passed = true;
  }
  UNPROTECT_LUA()

  EXPECT_EQ(passed, true);
}

TEST(Lua, Switches)
{
  luaExecStr("if MIXSRC_SA == nil then error('failed') end");
  luaExecStr("if MIXSRC_SB == nil then error('failed') end");
  luaExecStr("assert(getSwitchIndex('T1-') == nil)");
}

TEST(Lua, PhysicalSwitchCompatibility)
{
  luaExecStr("for _, name in ipairs({'ON', 'OFF', 'One', 'Tele', 'Act', 'Ltc', 'T1-', 'T1+'}) do assert(getSwitchIndex(name) == nil, name) end");
  luaExecStr("assert(getSwitchInfo(0) == nil and getSwitchInfo(-1) == nil and getSwitchInfo(32767) == nil)");
  for (int sw = 0; sw < switchGetMaxAllSwitches(); ++sw) {
    if (!isSourceAvailable(MIXSRC_FIRST_SWITCH + sw)) continue;
    char code[256];
    char name[32];
    getSwitchName(name, sw);
    snprintf(code, sizeof(code),
             "local s = getSwitchInfo(%d); assert(s and s.type == %d and s.name == '%s' and s.isCustomisableSwitch == %s)",
             MIXSRC_FIRST_SWITCH + sw, g_model.getSwitchType(sw), name,
             switchIsCustomSwitch(sw) ? "true" : "false");
    luaExecStr(code);
  }
}

TEST(Lua, testFloatIntegerEquality)
{
  // 0.5 is not an integer, so it must not equal 0 (regression #7587)
  // both directions asserted explicitly so the intent is obvious at a glance
  luaExecStr("if 0.5 == 0 then error('0.5 == 0') end");
  luaExecStr("if not (0.5 ~= 0) then error('0.5 ~= 0') end");
  luaExecStr("if 0.50 == 0 then error('0.50 == 0') end");
  luaExecStr("if not (0.50 ~= 0) then error('0.50 ~= 0') end");
  luaExecStr("if 0.50 == 0.0 then error('0.50 == 0.0') end");
  // ... even when the value comes from a variable, as in the reported issue
  luaExecStr("local v = 0.50; if v == 0 then error('v == 0') end");
  luaExecStr("local v = 0.50; if not (v ~= 0) then error('v ~= 0') end");
  // negative non-integral floats must not equal integers either
  luaExecStr("if -0.5 == 0 then error('-0.5 == 0') end");
  luaExecStr("if not (-0.5 ~= 0) then error('-0.5 ~= 0') end");
  luaExecStr("if not (-0.5 < 0) then error('-0.5 < 0') end");
  luaExecStr("if -0.5 > 0 then error('-0.5 > 0') end");
  // integral floats still compare equal to their integer counterpart
  luaExecStr("if 1.0 ~= 1 then error('1.0 ~= 1') end");
  luaExecStr("if 0.0 ~= 0 then error('0.0 ~= 0') end");
  luaExecStr("if -1.0 ~= -1 then error('-1.0 ~= -1') end");
  // order comparisons on the same values must remain consistent
  luaExecStr("if not (0.5 >= 0) then error('0.5 >= 0') end");
  luaExecStr("if 0.5 <= 0 then error('0.5 <= 0') end");
  luaExecStr("if not (0.5 > 0) then error('0.5 > 0') end");
  luaExecStr("if 0.5 < 0 then error('0.5 < 0') end");
  luaExecStr("if not (0.50 >= 0) then error('0.50 >= 0') end");
  luaExecStr("if 0.50 <= 0 then error('0.50 <= 0') end");
  luaExecStr("if not (1.0 >= 1) then error('1.0 >= 1') end");
  luaExecStr("if not (1.0 <= 1) then error('1.0 <= 1') end");
  luaExecStr("if not (0.0 >= 0) then error('0.0 >= 0') end");
  luaExecStr("if not (0.0 <= 0) then error('0.0 <= 0') end");
  // mixed arithmetic must still yield floats; the fix only affects equality
  luaExecStr("if math.type(0.5 + 0) ~= 'float' then error('0.5 + 0') end");
  luaExecStr("if math.type(1.0 + 1) ~= 'float' then error('1.0 + 1') end");
  luaExecStr("if math.type(1 / 2) ~= 'float' then error('1 / 2') end");
  luaExecStr("if math.type(7.5 % 2) ~= 'float' then error('7.5 % 2') end");
  luaExecStr("if math.type(0.5 * 2) ~= 'float' then error('0.5 * 2') end");
}

TEST(Lua, testLegacyNames)
{
  MODEL_RESET();
  for (uint8_t i = 0; i < 4; i ++)
    anaSetFiltered(i, -1024);
  evalAnalogControls();
  luaExecStr("value = getValue('thr')");
  luaExecStr("if value ~= -1024 then error('thr not defined in Legacy') end");
  luaExecStr("value = getValue('ail')");
  luaExecStr("if value ~= -1024 then error('ail not defined in Legacy') end");
  luaExecStr("value = getValue('rud')");
  luaExecStr("if value ~= -1024 then error('rud not defined in Legacy') end");
  luaExecStr("value = getValue('ele')");
  luaExecStr("if value ~= -1024 then error('ele not defined in Legacy') end");
}

TEST(Lua, ioSeek)
{
  const char io_seek_tst[] =
      "local file_name = \"seek-test.txt\"\n"
      "local file = io.open(file_name, \"w\")\n"
      "io.write(file, \"abcd\")\n"
      "io.close(file)\n"
      "file = io.open(file_name, \"r\")\n"
      // the file should have 4 characters
      "assert(#io.read(file, 32) == 4)\n"
      // io.seek() should return 0 if it is successful
      "assert(io.seek(file, 2) == 0)\n"
      "local r = io.read(file, 32)\n"
      // if reading from position 2,
      // we should read 2 characters,
      "assert(#r == 2)\n";

  luaExecStr(io_seek_tst);
  std::filesystem::remove(simuFatfsGetRealPath("seek-test.txt"));
}

#endif   // #if defined(LUA)

TEST(Lua, LogicalSwitchApisRemoved)
{
  luaExecStr("assert(model.getLogicalSwitch == nil and model.setLogicalSwitch == nil)");
  luaExecStr("assert(getLogicalSwitchValue == nil and setStickySwitch == nil)");
  luaExecStr("assert(LS_FUNC_AND == nil and getSwitchIndex('L01') == nil)");
  luaExecStr("assert(type(getSwitchName) == 'function' and type(getSwitchValue) == 'function')");
}

TEST(Lua, ActionConfigurationApisRemoved)
{
  luaExecStr("assert(model.getCustomFunction == nil and model.setCustomFunction == nil)");
  luaExecStr("assert(FUNC_OVERRIDE_CHANNEL == nil and FUNC_PLAY_SCRIPT == nil)");
  luaExecStr("assert(type(playFile) == 'function' and type(screenshot) == 'function')");
  luaExecStr("assert(type(model.setTimer) == 'function' and type(loadScript) == 'function')");
}

TEST(Lua, MixerConfigurationApisRemoved)
{
  luaExecStr("assert(model.deleteMixes == nil and model.insertMix == nil and model.getMix == nil and model.getMixesCount == nil and model.deleteMix == nil)");
}

TEST(Lua, OutputValueBounds)
{
  channelOutputs[0] = 321;
  channelOutputs[MAX_OUTPUT_CHANNELS - 1] = -456;
  luaExecStr("assert(getOutputValue(0) == 321)");
  luaExecStr(("assert(getOutputValue(" + std::to_string(MAX_OUTPUT_CHANNELS - 1) + ") == -456)").c_str());
  for (int id : {-65536, -32768, -1, MAX_OUTPUT_CHANNELS, 32767, 65536})
    luaExecStr(("assert(getOutputValue(" + std::to_string(id) + ") == 0)").c_str());
}

TEST(Lua, NumericSourceBounds)
{
  // Lua itself rejects values outside its 32-bit integer representation.
  luaExecStr("assert(not pcall(getValue, 4294967297))");
  luaExecStr("assert(not pcall(getSourceValue, -4294967297))");
  for (const char* id : {"-2147483648", "2147483647"}) {
    luaExecStr((std::string("assert(getValue(") + id + ") == 0)").c_str());
    luaExecStr((std::string("assert(getSourceValue(") + id + ") == nil)").c_str());
  }
}

#if defined(CROSSFIRE)
#include "telemetry/crsf_device.h"
#include "telemetry/crossfire.h"
#include "crc.h"

TEST(Lua, DeviceRequestsAreValidatedAndAtomic)
{
  auto oldInternal = moduleState[INTERNAL_MODULE].protocol;
  auto oldExternal = moduleState[EXTERNAL_MODULE].protocol;
  moduleState[INTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_CROSSFIRE;
  moduleState[EXTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_CROSSFIRE;
  CrsfDevice::cancel();
  // Traffic used by ExpressLRS: broadcast discovery, chunk read, command write.
  for (const char* request : {
         "assert(crossfireTelemetryPush(0x28, {0, 0xEA}))",
         "assert(crossfireTelemetryPush(0x2C, {0xEE, 0xEA, 1, 0}))",
         "assert(crossfireTelemetryPush(0x2D, {0xEE, 0xEA, 1, 6}))"}) {
    luaExecStr(request);
    uint8_t frame[64] = {};
    size_t size = CrsfDevice::take(INTERNAL_MODULE, frame, sizeof(frame));
    ASSERT_GE(size, 6u);
    EXPECT_EQ(MODULE_ADDRESS, frame[0]);
    EXPECT_EQ(RADIO_ADDRESS, frame[4]);
    EXPECT_EQ(size - 2, frame[1]);
    EXPECT_EQ(crc8(frame + 2, size - 3), frame[size - 1]);
  }
  // ELRS 3.x's dedicated Lua origin is preserved, only for TX requests.
  luaExecStr("assert(crossfireTelemetryPush(0x2C,{0xEE,0xEF,1,0}))");
  uint8_t legacyFrame[64];
  EXPECT_EQ(8u, CrsfDevice::take(INTERNAL_MODULE, legacyFrame, sizeof(legacyFrame)));
  EXPECT_EQ(0xEF, legacyFrame[4]);
  luaExecStr("assert(not crossfireTelemetryPush(0x2C,{0xEC,0xEF,1,0}))");
  luaExecStr("for _, t in ipairs({0x16,0x17,0x32,0x116,-1,256}) do "
             "assert(not crossfireTelemetryPush(t,{0xEE,0xEA,1,0})) end");
  luaExecStr("for _, p in ipairs({{}, {0,0xEA,1,0}, {0xEA,0xEA,1,0}, "
             "{0xEE,0,1,0}, {0xEE,0xEA,1,256}, {0xEE,0xEA,1,-1}, "
             "{0xEE,0xEA,1,'bad'}, {0xEE,0xEA,1,1.5}}) do "
             "assert(not crossfireTelemetryPush(0x2D,p)) end");
  luaExecStr("assert(crossfireTelemetryPush())");
  // Raw table lengths of 61 and 256 previously overflowed or wrapped uint8_t.
  luaExecStr("for _, n in ipairs({61,256,4096}) do local p={0xEE,0xEA}; "
             "for i=3,n do p[i]=1 end; assert(not crossfireTelemetryPush(0x2D,p)); "
             "assert(crossfireTelemetryPush()) end");
  luaExecStr("local p={0xEE,0xEA}; for i=3,60 do p[i]=1 end; "
             "assert(crossfireTelemetryPush(0x2D,p)); "
             "assert(not crossfireTelemetryPush(0x28,{0,0xEA}))");
  uint8_t frame[66];
  memset(frame, 0xA5, sizeof(frame));
  EXPECT_EQ(64u, CrsfDevice::take(INTERNAL_MODULE, frame + 1, 64));
  EXPECT_EQ(0xA5, frame[0]);
  EXPECT_EQ(0xA5, frame[65]);
  EXPECT_EQ(62, frame[2]);
  EXPECT_EQ(crc8(frame + 3, 61), frame[64]);
  extern lua_State* lsScripts;
  int top = lua_gettop(lsScripts);
  for (int i = 0; i < 100; ++i) {
    luaExecStr("assert(crossfireTelemetryPush(0x28,{0,0xEA}))");
    CrsfDevice::cancel();
  }
  EXPECT_EQ(top, lua_gettop(lsScripts));
  moduleState[INTERNAL_MODULE].protocol = oldInternal;
  moduleState[EXTERNAL_MODULE].protocol = oldExternal;
}

TEST(Lua, ControlConfigurationIsReadOnly)
{
  auto oldFilter = g_model.jitterFilter;
  auto oldModule = g_model.moduleData[0];
  luaExecStr("assert(model.setModule == nil and type(model.getModule) == 'function')");
  luaExecStr("model.setInfo({jitterFilter=2})");
  EXPECT_EQ(oldFilter, g_model.jitterFilter);
  EXPECT_EQ(0, memcmp(&oldModule, &g_model.moduleData[0], sizeof(oldModule)));
}
#endif

#if defined(CROSSFIRE)
TEST(Lua, DeviceResponsesReachTheToolThroughRuntime)
{
  // Pop allocates the existing per-tool queue. This path is shared by the
  // decoder and covers discovery, parameter chunks and command responses.
  luaExecStr("while crossfireTelemetryPop() do end");
  uint8_t discovery[] = {7, 0x29, 0xEA, 0xEE, 'T', 'X', 0};
  LuaRuntime::receiveTelemetry(discovery, sizeof(discovery));
  luaExecStr("local t,p=crossfireTelemetryPop(); assert(t==0x29 and #p==5 "
             "and p[1]==0xEA and p[2]==0xEE and p[3]==84)");
  uint8_t parameter[] = {7, 0x2B, 0xEF, 0xEE, 1, 0, 6};
  LuaRuntime::receiveTelemetry(parameter, sizeof(parameter));
  luaExecStr("local t,p=crossfireTelemetryPop(); assert(t==0x2B and #p==5 "
             "and p[1]==0xEF and p[2]==0xEE and p[3]==1 and p[5]==6); "
             "assert(crossfireTelemetryPop()==nil)");
}
#endif

#if defined(LUA) && defined(CROSSFIRE)
TEST(Lua, ClosingRuntimeCancelsDeviceRequestAndOldReplies)
{
  auto oldInternal = moduleState[INTERNAL_MODULE].protocol;
  auto oldExternal = moduleState[EXTERNAL_MODULE].protocol;
  moduleState[INTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_CROSSFIRE;
  moduleState[EXTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_NONE;
  CrsfDevice::cancel();
  luaExecStr("while crossfireTelemetryPop() do end; "
             "assert(crossfireTelemetryPush(0x2C,{0xEE,0xEA,1,0}))");
  uint8_t response[] = {7, 0x2B, 0xEA, 0xEE, 1, 0, 6};
  LuaRuntime::receiveTelemetry(response, sizeof(response));
  LuaRuntime::shutdown();
  uint8_t frame[64];
  EXPECT_EQ(0u, CrsfDevice::take(INTERNAL_MODULE, frame, sizeof(frame)));
  EXPECT_TRUE(CrsfDevice::available());
  luaExecStr("assert(crossfireTelemetryPop()==nil)");
  moduleState[INTERNAL_MODULE].protocol = oldInternal;
  moduleState[EXTERNAL_MODULE].protocol = oldExternal;
}
#endif
