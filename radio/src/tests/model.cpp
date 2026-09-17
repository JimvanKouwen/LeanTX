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
#include "model_yaml_test.h"

TEST(Model, RetiredOutputSettingsAreIgnoredAndNotSaved)
{
  MODEL_RESET();
  loadModelYamlStr(
      "extendedLimits: 1\n"
      "mixData:\n  0:\n    destCh: 0\n    srcRaw: MAX\n    weight: 100\n"
      "limitData:\n  0:\n    min: 500\n    max: -500\n"
      "    offset: 250\n    revert: 1\n    symetrical: 1\n"
      "    ppmCenter: 100\n    name: Old\n    curve: 1\n");
  updateChannelOutputs();
  EXPECT_EQ(0, channelOutputs[0]);
  const auto yaml = saveModelYamlStr(g_model);
  EXPECT_EQ(std::string::npos, yaml.find("limitData:"));
  EXPECT_EQ(std::string::npos, yaml.find("extendedLimits:"));
}

static const char* _model_config[] =
  {
    // As written by radio firmware - always enclosed in double quotes
    "header: \n"
    "   name: \"Tst Name\"\n",         // no embedded double quote

    "header: \n"
    "   name: \"Tst \\x22 Name\"\n",   // embedded and encoded double quote

    // As written by Companion - only enclosed in double quotes when necessary
    "header: \n"
    "   name: Tst Name\n",             // no embedded double quote

    "header: \n"
    "   name: Tst \" Name\n",          // embedded double quote in string

    "header: \n"
    "   name: \"\\\"Tst Name\"\n",     // embedded double quote at start of string
  };

static char* modelName()
{
  static char name[LEN_MODEL_NAME + 1];
  strncpy(name, g_model.header.name, LEN_MODEL_NAME);
  name[LEN_MODEL_NAME] = 0;
  return name;
}

TEST(Model, testModelNameParse)
{
  loadModelYamlStr(_model_config[0]);
  EXPECT_STREQ(modelName(), "Tst Name");
  loadModelYamlStr(_model_config[1]);
  EXPECT_STREQ(modelName(), "Tst \" Name");
  loadModelYamlStr(_model_config[2]);
  EXPECT_STREQ(modelName(), "Tst Name");
  loadModelYamlStr(_model_config[3]);
  EXPECT_STREQ(modelName(), "Tst \" Name");
  loadModelYamlStr(_model_config[4]);
  EXPECT_STREQ(modelName(), "\"Tst Name");
}

extern uint8_t getRequiredProtocol(uint8_t module);

// Loading a removed or unrecognized protocol must also turn an already active
// module off; it must never retain the previous model's RF selection.
TEST(Model, UnknownRfModuleIsOff)
{
  const char* types[] = {"TYPE_PPM", "TYPE_XJT_PXX1", "TYPE_ISRM_PXX2",
                         "TYPE_DSM2", "TYPE_MULTIMODULE", "TYPE_GHOST",
                         "TYPE_AFHDS3", "unknown", "5", "261"};
  for (const char* type : types) {
    for (int module = 0; module < NUM_MODULES; ++module) {
      SCOPED_TRACE(type);
      SCOPED_TRACE(module);
      g_model.moduleData[module].type = MODULE_TYPE_CROSSFIRE;
      char yaml[128];
      snprintf(yaml, sizeof(yaml), "moduleData:\n  %d:\n    type: %s\n", module, type);
      loadModelYamlStr(yaml);
      EXPECT_EQ(MODULE_TYPE_NONE, g_model.moduleData[module].type);
      EXPECT_EQ(PROTOCOL_CHANNELS_NONE, getRequiredProtocol(module));
    }
  }
}

TEST(Model, CrossfireRfModuleLoads)
{
  memset(&g_model, 0, sizeof(g_model));
  loadModelYamlStr("moduleData:\n  0:\n    type: TYPE_CROSSFIRE\n"
                   "  1:\n    type: TYPE_CROSSFIRE\n");
  for (int module = 0; module < NUM_MODULES; ++module)
    EXPECT_EQ(MODULE_TYPE_CROSSFIRE, g_model.moduleData[module].type);
}

TEST(Model, UnsupportedRfTypesCannotStartDriver)
{
  memset(&g_model, 0, sizeof(g_model));
  for (int module = 0; module < NUM_MODULES; ++module) {
    for (int type = 0; type < 64; ++type) {
      if (type == MODULE_TYPE_CROSSFIRE) continue;
      g_model.moduleData[module].type = type;
      EXPECT_EQ(PROTOCOL_CHANNELS_NONE, getRequiredProtocol(module));
    }
    for (int type : {-1, 1, 4, 6, 15, 255, 261}) {
      setModuleType(module, type);
      EXPECT_EQ(MODULE_TYPE_NONE, g_model.moduleData[module].type);
      EXPECT_EQ(0, sentModuleChannels(module));
    }
    setModuleType(module, MODULE_TYPE_CROSSFIRE);
    EXPECT_EQ(MODULE_TYPE_CROSSFIRE, g_model.moduleData[module].type);
    EXPECT_EQ(16, sentModuleChannels(module));
  }
}

#if defined(HARDWARE_EXTERNAL_MODULE)
TEST(Model, CrossfireRfDriverIsAvailable)
{
  memset(&g_model, 0, sizeof(g_model));
  setModuleType(EXTERNAL_MODULE, MODULE_TYPE_CROSSFIRE);
  EXPECT_EQ(PROTOCOL_CHANNELS_CROSSFIRE, getRequiredProtocol(EXTERNAL_MODULE));
  setModuleType(EXTERNAL_MODULE, MODULE_TYPE_NONE);
  EXPECT_EQ(PROTOCOL_CHANNELS_NONE, getRequiredProtocol(EXTERNAL_MODULE));
}
#endif

TEST(Model, MissingRfTypeDefaultsOff)
{
  // loadModel() clears g_model before parsing. Empty YAML values are omitted
  // by the parser, so they retain this RF-off default.
  memset(&g_model, 0, sizeof(g_model));
  loadModelYamlStr("moduleData:\n  0:\n    type:\n  1:\n    type: \"\"\n");
  for (int module = 0; module < NUM_MODULES; ++module)
    EXPECT_EQ(PROTOCOL_CHANNELS_NONE, getRequiredProtocol(module));
}

TEST(Model, RemovedTrainerSettingsAreIgnored)
{
  memset(&g_model, 0, sizeof(g_model));
  loadModelYamlStr("trainerData:\n  mode: MASTER_JACK\n  channelsCount: 8\n"
                   "radioTrainerDisabled: 1\n"
                   "header:\n  name: Local\n"
                   "moduleData:\n  1:\n    type: TYPE_CROSSFIRE\n");
  EXPECT_STREQ(modelName(), "Local");
  EXPECT_EQ(MODULE_TYPE_CROSSFIRE, g_model.moduleData[1].type);
}

TEST(Model, CompactCrsfSettingsRoundTrip)
{
  memset(&g_model, 0, sizeof(g_model));
  for (int module = 0; module < NUM_MODULES; ++module) {
    auto& md = g_model.moduleData[module];
    md.type = MODULE_TYPE_CROSSFIRE;
    md.channelsStart = module + 2;
    md.channelsCount = 8;
    md.crsf.telemetryBaudrate = 3;
    md.crsf.crsfArmingMode = ARMING_MODE_SWITCH;
    md.crsf.crsfArmingCondition = physicalSwitchCondition(0, 2);
  }
  auto& voltage = g_model.telemetrySensors[0];
  memcpy(voltage.label, "Volt", 4);
  voltage.id = 8;
  voltage.instance = TELEMETRY_ENDPOINT_SPORT;
  voltage.unit = UNIT_VOLTS;
  voltage.prec = 1;
  auto& calculated = g_model.telemetrySensors[1];
  memcpy(calculated.label, "Calc", 4);
  calculated.type = TELEM_TYPE_CALCULATED;
  calculated.formula = TELEM_FORMULA_ADD;
  calculated.calc.sources[0] = 1;
  g_model.rfAlarms.warning = 45;
  g_model.rfAlarms.critical = 42;

  std::string yaml = saveModelYamlStr(g_model);

  memset(&g_model, 0, sizeof(g_model));
  loadModelYamlStr(yaml.c_str());
  for (int module = 0; module < NUM_MODULES; ++module) {
    const auto& md = g_model.moduleData[module];
    EXPECT_EQ(MODULE_TYPE_CROSSFIRE, md.type);
    EXPECT_EQ(module + 2, md.channelsStart);
    EXPECT_EQ(8, md.channelsCount);
    EXPECT_EQ(3, md.crsf.telemetryBaudrate);
    EXPECT_EQ(ARMING_MODE_SWITCH, md.crsf.crsfArmingMode);
    EXPECT_EQ(physicalSwitchCondition(0, 2), md.crsf.crsfArmingCondition);
  }
  EXPECT_EQ(8, g_model.telemetrySensors[0].id);
  EXPECT_EQ(TELEMETRY_ENDPOINT_SPORT, g_model.telemetrySensors[0].instance);
  EXPECT_EQ(TELEM_FORMULA_ADD, g_model.telemetrySensors[1].formula);
  EXPECT_EQ(1, g_model.telemetrySensors[1].calc.sources[0]);
  EXPECT_EQ(45, g_model.rfAlarms.warning);
  EXPECT_EQ(42, g_model.rfAlarms.critical);
}

TEST(Model, RemovedTrimSettingsAreIgnored)
{
  memset(&g_model, 0, sizeof(g_model));
  loadModelYamlStr(
      "thrTrim: 1\ndisplayTrims: 2\ntrimInc: 2\nextendedTrims: 1\nthrTrimSw: 3\n"
      "flightModeData:\n  0:\n    name: Main\n    trim:\n      0:\n"
      "        value: 512\n        mode: 3\n    fadeIn: 7\n"
      "header:\n  name: Buttons\n");
  EXPECT_STREQ(modelName(), "Buttons");
  EXPECT_EQ(0, g_model.reservedThrTrim);
  EXPECT_EQ(0, g_model.reservedExtendedTrims);

  std::string yaml = saveModelYamlStr(g_model);
  for (const char* field : {"thrTrim:", "displayTrims:", "trimInc:",
                            "extendedTrims:", "thrTrimSw:", "trim:",
                            "flightModeData:", "flightModes:", "reservedTrims:", "trimSource:", "carryTrim:"})
    EXPECT_EQ(std::string::npos, yaml.find(field)) << field;
}

TEST(Model, RemovedActionSettingsAreIgnored)
{
  MODEL_RESET();
  RADIO_RESET();
  const char* obsolete =
      "customFn:\n  0:\n    swtch: ON\n    func: OVERRIDE_CHANNEL\n"
      "    def: 0,100,1\nnoGlobalFunctions: 1\n"
      "radioGFDisabled: 1\nmodelSFDisabled: 1\n";
  {
    std::string input = obsolete;
    input += "header:\n  name: NoActions\n";
    loadModelYamlStr(input.c_str());
    EXPECT_STREQ("NoActions", modelName());
    std::string output = saveModelYamlStr(g_model);
    for (const char* field : {"customFn:", "noGlobalFunctions:",
                              "radioGFDisabled:", "modelSFDisabled:"})
      EXPECT_EQ(std::string::npos, output.find(field)) << field;
  }
}

TEST(Model, RemovedLineConditionsAreIgnored)
{
  memset(&g_model, 0, sizeof(g_model));
  loadModelYamlStr(
      "flightModeData:\n  0:\n    gvars:\n      0: 999\n"
      "mixData:\n -\n    destCh: 4\n    srcRaw: MAX\n"
      "    weight: 50\n    swtch: ON\n    flightModes: 111111111\n"
      "    mltpx: REPL\n    mixWarn: 3\n    delayUp: 10\n    speedDown: 10\n"
      "");
  updateChannelOutputs();
  EXPECT_EQ(0, channelOutputs[4]);
  const auto yaml = saveModelYamlStr(g_model);
  for (const char* field : {"mltpx:", "mixWarn:", "delayUp:", "speedDown:", "flightModes:"})
    EXPECT_EQ(std::string::npos, yaml.find(field));
}


TEST(Model, PhysicalSwitchSourceRoundTrip)
{
  SYSTEM_RESET();
  MODEL_RESET();
  RADIO_RESET();
  const int sw = findHwSwitch(SWITCH_3POS);
  ASSERT_GE(sw, 0);
  loadModelYamlStr("logicalSw:\n  0:\n    func: AND\n    def: SA0,SB0\n");

  g_model.channelMappings[0].source = physicalSwitch(sw);
  std::string yaml = saveModelYamlStr(g_model);
  EXPECT_EQ(std::string::npos, yaml.find("logicalSw"));
  memset(&g_model, 0, sizeof(g_model));
  loadModelYamlStr(yaml.c_str());
  EXPECT_EQ(physicalSwitch(sw), g_model.channelMappings[0].source);
}
