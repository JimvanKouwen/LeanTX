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

#include "storage/yaml/yaml_tree_walker.h"
#include "storage/yaml/yaml_parser.h"
#include "storage/yaml/yaml_datastructs.h"
#include "storage/yaml/yaml_bits.h"

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

static void loadModelYamlStr(const char* str)
{
  YamlTreeWalker tree;
  tree.reset(get_modeldata_nodes(), (uint8_t*)&g_model);

  YamlParser yp;
  yp.init(YamlTreeWalker::get_parser_calls(), &tree);

  size_t len = strlen(str);
  yp.parse(str, len);
}

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
    md.crsf.crsfArmingTrigger = SWSRC_ON;
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

  std::string yaml;
  YamlTreeWalker tree;
  tree.reset(get_modeldata_nodes(), (uint8_t*)&g_model);
  ASSERT_TRUE(tree.generate([](void* opaque, const char* str, size_t len) {
    static_cast<std::string*>(opaque)->append(str, len);
    return true;
  }, &yaml));

  memset(&g_model, 0, sizeof(g_model));
  loadModelYamlStr(yaml.c_str());
  for (int module = 0; module < NUM_MODULES; ++module) {
    const auto& md = g_model.moduleData[module];
    EXPECT_EQ(MODULE_TYPE_CROSSFIRE, md.type);
    EXPECT_EQ(module + 2, md.channelsStart);
    EXPECT_EQ(8, md.channelsCount);
    EXPECT_EQ(3, md.crsf.telemetryBaudrate);
    EXPECT_EQ(ARMING_MODE_SWITCH, md.crsf.crsfArmingMode);
    EXPECT_EQ(SWSRC_ON, md.crsf.crsfArmingTrigger);
  }
  EXPECT_EQ(8, g_model.telemetrySensors[0].id);
  EXPECT_EQ(TELEMETRY_ENDPOINT_SPORT, g_model.telemetrySensors[0].instance);
  EXPECT_EQ(TELEM_FORMULA_ADD, g_model.telemetrySensors[1].formula);
  EXPECT_EQ(1, g_model.telemetrySensors[1].calc.sources[0]);
  EXPECT_EQ(45, g_model.rfAlarms.warning);
  EXPECT_EQ(42, g_model.rfAlarms.critical);
}
