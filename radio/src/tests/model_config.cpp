// LeanTX model storage tests. GPL-2.0-or-later.
#include <string>

#include "edgetx.h"
#include "gtest/gtest.h"
#include "storage/model_config_adapter.h"
namespace
{
struct ModelConfig : testing::Test {
  config_stream::Workspace workspace;
  std::string input, output;
  size_t position = 0;
  static int read(void* p)
  {
    auto& t = *static_cast<ModelConfig*>(p);
    return t.position < t.input.size() ? (unsigned char)t.input[t.position++]
                                       : -1;
  }
  static bool write(void* p, const char* s, size_t n)
  {
    static_cast<ModelConfig*>(p)->output.append(s, n);
    return true;
  }
  config_stream::Result load(ModelData& m)
  {
    position = 0;
    auto copy = input;
    auto r = config_stream::parse(copy.data(), copy.size(), model_config::beginLoad(), workspace);
    if (r && r.invalid) r.error = "invalid model value";
    if (r) r.error = model_config::resolveLoad();
    if (r) model_config::commitLoad(m);
    return r;
  }
  config_stream::Result save(ModelData& m)
  {
    position = 0;
    output.clear();
    return config_stream::process({this, read, nullptr}, {this, nullptr, write},
                                  model_config::document(m), workspace, false);
  }
};
TEST_F(ModelConfig, Roundtrip)
{
  ModelData a{};
  a.rfAlarms.warning = 45;
  a.rfAlarms.critical = 42;
  strcpy(a.header.name, "Test");
  a.header.modelId[0] = 42;
  a.mixData[0].srcRaw = MIXSRC_FIRST_STICK;
  a.mixData[0].weight = 81;

  a.timers[0].start = 600;
  a.telemetrySensors[0].type = TELEM_TYPE_CALCULATED;
  a.telemetrySensors[0].formula = TELEM_FORMULA_CELL;
  a.telemetrySensors[0].cell.source = 4;
  a.telemetrySensors[0].cell.index = 2;
  ASSERT_TRUE(save(a));
  input = output;
  ModelData b{};
  auto r = load(b);
  ASSERT_TRUE(r) << r.error;
  EXPECT_EQ(0u, r.invalid);
  EXPECT_STREQ("Test", b.header.name);
  EXPECT_EQ(81, b.mixData[0].weight);
  EXPECT_EQ(4, b.telemetrySensors[0].cell.source);
  EXPECT_EQ(600u, b.timers[0].start);
}
TEST_F(ModelConfig, LegacyMixerTimingIsDiscarded)
{
  input = "mixData:\n  - srcRaw: Rud\n    weight: 75\n    offset: 12\n"
          "    delayPrec: 1\n    speedPrec: 1\n"
          "    delayUp: 250\n    delayDown: 250\n"
          "    speedUp: 250\n    speedDown: 250\n";
  ModelData model{};
  ASSERT_TRUE(load(model));
  EXPECT_EQ(MIXSRC_FIRST_STICK, model.mixData[0].srcRaw);
  EXPECT_EQ(75, model.mixData[0].weight);
  EXPECT_EQ(12, model.mixData[0].offset);
  ASSERT_TRUE(save(model));
  for (const char* field : {"delayPrec:", "speedPrec:", "delayUp:",
                            "delayDown:", "speedUp:", "speedDown:"}) {
    EXPECT_EQ(std::string::npos, output.find(field)) << field;
  }
}

TEST_F(ModelConfig, UnknownAndUnavailable)
{
  input =
      "header:\n  name: test\nfuture:\n  nested:\n    - x: [1, 2]\n      "
      "other: yes\nscriptsData:\n  8:\n    file: future\n";
  ModelData m{};
  auto r = load(m);
  ASSERT_TRUE(r) << r.error;
  ASSERT_TRUE(save(m));
  EXPECT_EQ(std::string::npos,
            output.find("    - x: [1, 2]\n      other: yes"));
  EXPECT_EQ(std::string::npos, output.find("    file: future"));
  EXPECT_EQ(45, m.rfAlarms.warning);
}
TEST_F(ModelConfig, LuaMixerSourcesAreRejected)
{
  for (const char* source : {"lua(0,0)", "lua(8,5)", "!lua(0,0)", "LUA1a"}) {
    input = std::string("mixData:\n  0:\n    srcRaw: ") + source + "\n";
    ModelData model{};
    strcpy(model.header.name, "unchanged");
    EXPECT_FALSE(load(model)) << source;
    EXPECT_STREQ("unchanged", model.header.name);
  }
}

TEST_F(ModelConfig, MalformedDoesNotCommit)
{
  ModelData m{};
  m.timers[0].start = 123;
  input = "timers:\n  0:\n    start: 500\nbroken: [\n";
  EXPECT_FALSE(load(m));
  EXPECT_EQ(123u, m.timers[0].start);
}
}  // namespace

#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "gtests.h"
#include "location.h"
#include "storage/model_config_file.h"
#include "storage/modelslist.h"
#include "storage/storage.h"
#include "legacy_fixture.h"
namespace
{
TEST_F(ModelConfig, LegacyFullModelRoundtrip)
{
  g_model = ModelData{};
  g_model.rfAlarms.warning = 45;
  g_model.rfAlarms.critical = 42;
  strcpy(g_model.header.name, "Legacy");
  g_model.header.modelId[1] = 72;
  for (unsigned i = 0; i < MAX_TIMERS; ++i) {
    g_model.timers[i].mode = TMRMODE_ON;
    g_model.timers[i].start = 1234 + i;
    g_model.timers[i].value = -25;
  }
  for (unsigned i = 0; i < MAX_MIXERS; ++i) {
    auto& m = g_model.mixData[i];
    m.srcRaw = MIXSRC_FIRST_STICK + i % MAX_STICKS;
    m.weight = i;
    m.offset = -int(i);
    m.destCh = i % MAX_OUTPUT_CHANNELS;

  }

  for (unsigned i = 0; i < MAX_OUTPUT_CHANNELS; ++i) {
    auto& o = g_model.limitData[i];
    o.min = -50;
    o.max = 80;
    o.offset = -34;
    o.ppmCenter = 5;
    o.revert = i % 2;
  }
  for (unsigned i = 0; i < MAX_TELEMETRY_SENSORS; ++i) {
    auto& s = g_model.telemetrySensors[i];
    s.id = 400 + i;
    s.instance = i;
    s.custom.ratio = 100;
    s.custom.offset = -15;
    s.unit = UNIT_VOLTS;
    s.prec = 2;
    strcpy(s.label, "V");
  }
  g_model.moduleData[1].type = MODULE_TYPE_CROSSFIRE;
  g_model.moduleData[1].channelsCount = 8;
  g_model.moduleData[1].crsf.crsfArmingTrigger = SWSRC_ON;
#if !defined(COLORLCD)
  g_model.setTelemetryScreenType(0, TELEMETRY_SCREEN_TYPE_BARS);
  g_model.screens[0].bars[0].source = MIXSRC_FIRST_TELEM;
  g_model.screens[0].bars[0].barMin = -30;
  g_model.screens[0].bars[0].barMax = 100;
#endif
#if defined(COLORLCD)
  input = legacyFixture("full-model-color.yml");
#else
  input = legacyFixture("full-model-mono.yml");
#endif
  ASSERT_FALSE(input.empty());
  ModelData expected = g_model, actual{};
  auto r = load(actual);
  ASSERT_TRUE(r) << r.error;
  EXPECT_EQ(0,
            memcmp(expected.mixData, actual.mixData, sizeof(expected.mixData)));
  EXPECT_EQ(0, memcmp(expected.limitData, actual.limitData,
                      sizeof(expected.limitData)));
  EXPECT_EQ(0, memcmp(expected.telemetrySensors, actual.telemetrySensors,
                      sizeof(expected.telemetrySensors)));
  ASSERT_TRUE(save(actual));
  input = output;
  auto first = output;
  ModelData again{};
  r = load(again);
  ASSERT_TRUE(r) << r.error;
  ASSERT_TRUE(save(again));
  EXPECT_EQ(first, output);
}
TEST_F(ModelConfig, NestedListsAndDefaults)
{
  input =
      "timers:\n  - start: 321\n    future:\n      - nested: yes\nmixData:\n  "
      "- srcRaw: Rud\n    weight: 89\nscriptsData:\n  0:\n    file: mix\n    "
      "inputs:\n      0:\n        u:\n          source: ch(3)\n";
  ModelData m{};
  auto r = load(m);
  ASSERT_TRUE(r) << r.error;
  EXPECT_EQ(321u, m.timers[0].start);
  EXPECT_EQ(89, m.mixData[0].weight);
  ASSERT_TRUE(save(m));
  EXPECT_EQ(std::string::npos, output.find("      - nested: yes"));
  input = output;
  r = load(m);
  ASSERT_TRUE(r) << r.error;
  EXPECT_FALSE(r.missing);
}
TEST_F(ModelConfig, UnavailableIsKnown)
{
#if !defined(COLORLCD)
  input =
      "screenData:\n  0:\n    LayoutId: Layout1x1\n    layoutData:\n      "
      "zones:\n        0:\n          widgetName: LuaWidget\n          "
      "widgetData:\n            options:\n              0:\n                "
      "type: String\n                value:\n                  stringValue: "
      "preserved\n";
#else
  input =
      "screens:\n  0:\n    type: SCRIPT\n    u:\n      script:\n        file: "
      "mono\n";
#endif
  auto original = input;
  ModelData m{};
  auto r = load(m);
  ASSERT_TRUE(r) << r.error;
  EXPECT_EQ(0u, r.unknown);
  ASSERT_TRUE(save(m));
  EXPECT_EQ(std::string::npos, output.find(original));
}
TEST_F(ModelConfig, OrderIndependentTelemetry)
{
  input =
      "telemetrySensors:\n  0:\n    cfg:\n      cell:\n        source: 8\n     "
      "   index: 2\n    id2:\n      formula: FORMULA_CELL\n    type: "
      "TYPE_CALCULATED\n";
  ModelData m{};
  auto r = load(m);
  ASSERT_TRUE(r) << r.error;
  EXPECT_EQ(TELEM_FORMULA_CELL, m.telemetrySensors[0].formula);
  EXPECT_EQ(8, m.telemetrySensors[0].cell.source);
}
#if defined(COLORLCD)
TEST_F(ModelConfig, ScreenWidgetsAreTransactional)
{
  g_model.resetScreenData();
  g_model.setScreenLayoutId(0, "Original");
  input =
      "screenData:\n  0:\n    LayoutId: Layout1x1\n    layoutData:\n      "
      "zones:\n        0:\n          widgetName: LuaWidget\n          "
      "widgetData:\n            options:\n              0:\n                "
      "type: String\n                value:\n                  stringValue: "
      "test value\n";
  auto valid = input;
  input += "bad: [\n";
  EXPECT_FALSE(load(g_model));
  EXPECT_STREQ("Original", g_model.getScreenLayoutId(0));
  input = valid;
  auto r = load(g_model);
  ASSERT_TRUE(r) << r.error;
  EXPECT_STREQ("Layout1x1", g_model.getScreenLayoutId(0));
  EXPECT_EQ("test value", g_model.getWidgetData(0, 0)->getString(0));
  ASSERT_TRUE(save(g_model));
  input = output;
  r = load(g_model);
  ASSERT_TRUE(r) << r.error;
  EXPECT_EQ("test value", g_model.getWidgetData(0, 0)->getString(0));
}
#endif
class ModelConfigFile : public testing::Test
{
 protected:
  std::filesystem::path root;
  void SetUp() override
  {
    char path[] = "/tmp/leantx-model-config-XXXXXX";
    root = mkdtemp(path);
    std::filesystem::create_directory(root / "MODELS");
    simuFatfsSetPaths(root.c_str(), nullptr);
    g_model = ModelData{};
  }
  void TearDown() override
  {
    simuFatfsSetWriteBudget(-1);
    simuFatfsFailRenameAfter(-1);
    simuFatfsSetPaths(TESTS_PATH, nullptr);
    std::filesystem::remove_all(root);
  }
  void put(const std::string& s)
  {
    std::ofstream(root / "MODELS/test.yml") << s;
  }
  std::string get()
  {
    std::ifstream f(root / "MODELS/test.yml");
    return std::string(std::istreambuf_iterator<char>(f), {});
  }
};
TEST_F(ModelConfigFile, FailedWritesRetainOriginal)
{
  put("header:\n  name: Original\nfuture:\n  nested: [one, two]\n");
  auto original = get();
  simuFatfsSetWriteBudget(1);
  EXPECT_NE(nullptr, saveModelConfig("/MODELS/test.yml"));
  EXPECT_EQ(original, get());
  simuFatfsSetWriteBudget(-1);
  simuFatfsFailRenameAfter(1);
  EXPECT_NE(nullptr, saveModelConfig("/MODELS/test.yml"));
  EXPECT_EQ(original, get());
  simuFatfsFailRenameAfter(-1);
  EXPECT_EQ(nullptr, saveModelConfig("/MODELS/test.yml"));
}
TEST_F(ModelConfigFile, MalformedAndFailedTemporaryOpen)
{
  put("header:\n  name: Original\nbad: [\n");
  auto original = get();
  strcpy(g_model.header.name, "Active");
  EXPECT_NE(nullptr, loadModelConfig("/MODELS/test.yml", g_model));
  EXPECT_STREQ("Active", g_model.header.name);
  EXPECT_NE(nullptr, saveModelConfig("/MODELS/test.yml"));
  EXPECT_EQ(original, get());
  std::filesystem::create_directory(root / "MODELS/test.yml.tmp");
  EXPECT_NE(nullptr, saveModelConfig("/MODELS/test.yml"));
  EXPECT_EQ(original, get());
}
TEST_F(ModelConfigFile, HeaderSkipsLargeBodyAndDoesNotTouchModel)
{
  put("header:\n  name: Metadata\n  modelId: [7, 42]\nbody:\n  blob: " +
      std::string(12000, 'x') +
      "\nmoduleData:\n  1:\n    type: TYPE_CROSSFIRE\n");
  strcpy(g_model.header.name, "Active");
  model_config::Header header{};
  const char* error = loadModelConfigHeader("/MODELS/test.yml", header);
  ASSERT_EQ(nullptr, error) << (error ? error : "");
  EXPECT_STREQ("Metadata", header.header.name);
  EXPECT_EQ(42, header.header.modelId[1]);
  EXPECT_EQ(MODULE_TYPE_CROSSFIRE, header.moduleData[1].type);
  EXPECT_STREQ("Active", g_model.header.name);
}
TEST_F(ModelConfigFile, InterruptedReplacementRecovers)
{
  put("header:\n  name: Original\n");
  std::filesystem::rename(root / "MODELS/test.yml",
                          root / "MODELS/test.yml.swap");
  EXPECT_EQ(nullptr, loadModelConfig("/MODELS/test.yml", g_model));
  EXPECT_STREQ("Original", g_model.header.name);
}
}  // namespace
namespace
{
TEST_F(ModelConfig, ScalarLists)
{
  input = "header:\n  modelId:\n    - 7\n    - 9\n";
  ModelData m{};
  auto r = load(m);
  ASSERT_TRUE(r) << r.error;
  EXPECT_EQ(7, m.header.modelId[0]);
  EXPECT_EQ(9, m.header.modelId[1]);
  ASSERT_TRUE(save(m));
  input = output;
  r = load(m);
  ASSERT_TRUE(r) << r.error;
  EXPECT_EQ(7, m.header.modelId[0]);
  EXPECT_EQ(9, m.header.modelId[1]);
}
TEST_F(ModelConfig, LongKnownStringIsTruncatedOnSave)
{
  std::string name(LEN_MODEL_NAME + 5, 'a');
  input = "header:\n  name: '" + name + "'\n";
  ModelData m{};
  auto r = load(m);
  ASSERT_TRUE(r) << r.error;
  ASSERT_TRUE(save(m));
  EXPECT_EQ(std::string::npos, output.find(name));
  strcpy(m.header.name, "Edited");
  ASSERT_TRUE(save(m));
  EXPECT_EQ(std::string::npos, output.find(name));
}
}  // namespace
namespace
{
TEST_F(ModelConfig, UnavailableSourceWithComment)
{
  input = "mixData:\n  - srcRaw: P31 # larger radio\n    weight: 100\n";
  ModelData m{};
  auto r = load(m);
  ASSERT_TRUE(r) << r.error;
  EXPECT_EQ(0u, r.unknown);
  ASSERT_TRUE(save(m));
  EXPECT_EQ(std::string::npos, output.find("srcRaw: P31 # larger radio"));
  m.mixData[0].srcRaw = MIXSRC_FIRST_CH;
  ASSERT_TRUE(save(m));
  EXPECT_EQ(std::string::npos, output.find("P31"));
}
#if defined(COLORLCD)
TEST_F(ModelConfig, WidgetUnionResolvedAfterType)
{
  input =
      "screenData:\n  0:\n    LayoutId: Layout1x1\n    layoutData:\n      "
      "zones:\n        0:\n          widgetName: Test\n          widgetData:\n "
      "           options:\n              0:\n                value:\n         "
      "         signedValue: -17\n                  unsignedValue: 123\n       "
      "         type: Signed\n";
  auto r = load(g_model);
  ASSERT_TRUE(r) << r.error;
  EXPECT_EQ(-17, g_model.getWidgetData(0, 0)->getSignedValue(0));
  ASSERT_TRUE(save(g_model));
  input = output;
  r = load(g_model);
  ASSERT_TRUE(r) << r.error;
  EXPECT_EQ(-17, g_model.getWidgetData(0, 0)->getSignedValue(0));
}
#endif
}  // namespace
namespace
{
TEST_F(ModelConfig, UnavailableModuleDefaultsOffAndIsDroppedOnSave)
{
  input = "moduleData:\n  0:\n    type: TYPE_PPM\n";
  ModelData model{};
  model.moduleData[0].type = MODULE_TYPE_CROSSFIRE;
  auto result = load(model);
  ASSERT_TRUE(result) << result.error;
  EXPECT_EQ(MODULE_TYPE_NONE, model.moduleData[0].type);
  ASSERT_TRUE(save(model));
  EXPECT_EQ(std::string::npos, output.find("type: TYPE_PPM"));
}
#if defined(COLORLCD)
TEST_F(ModelConfig, LongWidgetString)
{
  const std::string value(300, 'x');
  input =
      "screenData:\n  0:\n    LayoutId: Layout1x1\n    layoutData:\n      "
      "zones:\n        0:\n          widgetName: Test\n          widgetData:\n "
      "           options:\n              0:\n                type: String\n   "
      "             value:\n                  stringValue: " +
      value + "\n";
  auto result = load(g_model);
  ASSERT_TRUE(result) << result.error;
  ASSERT_TRUE(save(g_model));
  input = output;
  result = load(g_model);
  ASSERT_TRUE(result) << result.error;
  EXPECT_EQ(value, g_model.getWidgetData(0, 0)->getString(0));
}
#endif
}  // namespace
#if defined(FUNCTION_SWITCHES)
#include "hal/switch_driver.h"
namespace
{
TEST_F(ModelConfig, FunctionSwitchIdentityAndUnavailableRecords)
{
  const char* physical = switchGetDefaultName(switchGetSwitchFromCustomIdx(1));
  input =
      "customSwitches:\n  - sw: SW99\n    name: future\n  - name: Test\n    "
      "sw: " +
      std::string(physical) + "\n    type: 2POS\n    group: 1\n";
  auto result = load(g_model);
  ASSERT_TRUE(result) << result.error;
  EXPECT_EQ(0, memcmp(g_model.customSwitches[1].name, "Test", LEN_SWITCH_NAME));
  ASSERT_TRUE(save(g_model));
  EXPECT_EQ(std::string::npos, output.find("sw: SW99\n    name: future"));
  input = output;
  result = load(g_model);
  ASSERT_TRUE(result) << result.error;
  EXPECT_EQ(SWITCH_2POS, g_model.customSwitches[1].type);
  EXPECT_EQ(1, g_model.customSwitches[1].group);
}
}  // namespace
#endif
namespace
{
TEST_F(ModelConfig, EmptyCollectionsUseDefaults)
{
  input = "header:\n  modelId: []\nmixData: []\ntimers: {}\n";
  ModelData model{};
  auto result = load(model);
  ASSERT_TRUE(result) << result.error;
  EXPECT_FALSE(result.missing);
  EXPECT_EQ(0, model.header.modelId[0]);
  ASSERT_TRUE(save(model));
  input = output;
  result = load(model);
  ASSERT_TRUE(result) << result.error;
  EXPECT_FALSE(result.missing);
}
}  // namespace
#if MAX_TELEMETRY_SENSORS < 99
namespace
{
TEST_F(ModelConfig, UnavailableTelemetryReferencesDefault)
{
  input =
      "varioData:\n  source: 98\ntelemetrySensors:\n  0:\n    type: "
      "TYPE_CALCULATED\n    id2:\n      formula: FORMULA_CELL\n    cfg:\n      "
      "cell:\n        source: 99\n";
  ModelData model{};
  auto result = load(model);
  ASSERT_TRUE(result) << result.error;
  EXPECT_EQ(0, model.varioData.source);
  EXPECT_EQ(0, model.telemetrySensors[0].cell.source);
  ASSERT_TRUE(save(model));
  EXPECT_EQ(std::string::npos, output.find("  source: 98"));
  EXPECT_EQ(std::string::npos, output.find("        source: 99"));
}
}  // namespace
#endif
namespace
{
TEST_F(ModelConfig, LegacyMixerScriptsAreDropped)
{
  input =
      "scriptsData:\n  0:\n    inputs:\n      0:\n        u:\n          "
      "source: lua(8,0)\n";
  ModelData model{};
  auto result = load(model);
  ASSERT_TRUE(result) << result.error;
  ASSERT_TRUE(save(model));
  EXPECT_EQ(std::string::npos, output.find("          source: lua(8,0)"));
  EXPECT_EQ(std::string::npos, output.find("scriptsData:"));
}
}  // namespace
namespace
{
TEST_F(ModelConfig, LegacyUnknownModuleTypesNeverEnableRf)
{
  const char* types[] = {"TYPE_PPM",
                         "TYPE_XJT_PXX1",
                         "TYPE_ISRM_PXX2",
                         "TYPE_DSM2",
                         "TYPE_MULTIMODULE",
                         "TYPE_GHOST",
                         "TYPE_AFHDS3",
                         "unknown",
                         "5",
                         "261",
                         "true"};
  for (auto type : types) {
    input = "moduleData:\n  0:\n    type: " + std::string(type) + "\n";
    ModelData model{};
    model.moduleData[0].type = MODULE_TYPE_CROSSFIRE;
    auto result = load(model);
    ASSERT_TRUE(result) << type << ": " << result.error;
    EXPECT_EQ(MODULE_TYPE_NONE, model.moduleData[0].type) << type;
  }
}
}  // namespace

namespace
{
TEST_F(ModelConfigFile, PocketModelHeaderAndContents)
{
  std::ifstream fixture(std::string(TESTS_PATH) + "/fixtures/pocket-model.yml");
  ASSERT_TRUE(fixture.is_open());
  const std::string yaml(std::istreambuf_iterator<char>(fixture), {});
  put(yaml);
  model_config::Header header{};
  ASSERT_EQ(nullptr, loadModelConfigHeader("/MODELS/test.yml", header));
  EXPECT_STREQ("POCKET", header.header.name);
  ASSERT_EQ(nullptr, loadModelConfig("/MODELS/test.yml", g_model));
  EXPECT_STREQ("POCKET", g_model.header.name);
  EXPECT_EQ(4185, g_model.timers[0].value);
  EXPECT_EQ(MODULE_TYPE_CROSSFIRE, g_model.moduleData[0].type);
  EXPECT_STREQ("RxBt", g_model.telemetrySensors[10].label);
  EXPECT_TRUE(modelConfigCanSave("/MODELS/test.yml"));
  EXPECT_EQ(nullptr, saveModelConfig("/MODELS/test.yml"));
  EXPECT_EQ(nullptr, loadModelConfig("/MODELS/test.yml", g_model));
  EXPECT_STREQ("POCKET", g_model.header.name);
}
}  // namespace

namespace {
TEST_F(ModelConfig, SparseSlotsKeepIndicesAndDefaultValues)
{
  ModelData model{};
  model.rfAlarms.warning = 45; model.rfAlarms.critical = 42;
  ASSERT_TRUE(save(model));
  for (const char* section : {"mixData:", "limitData:", "telemetrySensors:", "scriptsData:", "screens:", "screenData:"})
    EXPECT_EQ(std::string::npos, output.find(section)) << section;
  model.mixData[7].srcRaw = MIXSRC_FIRST_STICK;
  model.mixData[7].destCh = 3;
  model.mixData[7].weight = 100;
  model.limitData[11].offset = 123;
  model.timers[2].start = 50;
  // Stale data in inactive slots must not create entries.
  model.mixData[6].weight = 80;
  ASSERT_TRUE(save(model));
  EXPECT_EQ(std::string::npos, output.find("  6:"));
  EXPECT_EQ(std::string::npos, output.find("scriptsData:"));
  EXPECT_NE(std::string::npos, output.find("mixData:\n  7:"));
  EXPECT_NE(std::string::npos, output.find("limitData:\n  11:"));
  input = output;
  ModelData loaded{};
  ASSERT_TRUE(load(loaded));
  EXPECT_EQ(MIXSRC_FIRST_STICK, loaded.mixData[7].srcRaw);
  EXPECT_EQ(0, loaded.mixData[6].weight);
  EXPECT_EQ(123, loaded.limitData[11].offset);
  EXPECT_EQ(50u, loaded.timers[2].start);
  EXPECT_EQ(45, loaded.rfAlarms.warning);
}
TEST_F(ModelConfig, DuplicateAliasesAndInvalidScalarAreRejected)
{
  ModelData model{};
  for (const char* yaml : {"header:\n  modelId: [1, 2]\n  modelId:\n    0:\n      val: 3\n", "timers:\n  0:\n    start:\n"}) {
    input = yaml;
    EXPECT_FALSE(load(model));
  }
}
TEST_F(ModelConfigFile, WholeFileCapacityAndLastByte)
{
  const auto capacity = config_stream::DocumentCapacity;
  // Exactly capacity bytes, without a final newline, remains valid.
  std::string document = "header:\n  name: Boundary\n#";
  document.append(capacity - document.size(), 'x');
  put(document);
  ASSERT_EQ(nullptr, loadModelConfig("/MODELS/test.yml", g_model));
  EXPECT_STREQ("Boundary", g_model.header.name);
  put(document + "x");
  EXPECT_NE(nullptr, loadModelConfig("/MODELS/test.yml", g_model));
  EXPECT_STREQ("Boundary", g_model.header.name);
  EXPECT_EQ(document + "x", get());
  // Reset the failed-load guard with a valid load for subsequent tests.
  put("header:\n  name: Boundary\n");
  ASSERT_EQ(nullptr, loadModelConfig("/MODELS/test.yml", g_model));
}
TEST_F(ModelConfigFile, OversizedSaveLeavesOriginal)
{
  put("header:\n  name: Original\n");
  auto original = get();
  // A dense meaningful model exceeds the deliberately fixed 16 KiB document.
  for (unsigned i = 0; i < MAX_MIXERS; ++i) {
    auto& mix = g_model.mixData[i];
    mix.srcRaw = MIXSRC_FIRST_STICK; mix.weight = 100; mix.offset = 99;
    mix.destCh = 15;
    memset(mix.name, 'M', sizeof(mix.name));
  }

  for (auto& limit : g_model.limitData) {
    memset(limit.name, '"', sizeof(limit.name));
    limit.offset = 100;
    limit.min = -100;
    limit.max = 100;
    limit.ppmCenter = 100;
    limit.revert = 1;
    limit.symetrical = 1;
  }
  for (auto& sensor : g_model.telemetrySensors) {
    memset(sensor.label, 'S', sizeof(sensor.label));
    sensor.id = 1234;
    sensor.instance = 3;
    sensor.subId = 2;
    sensor.unit = UNIT_VOLTS;
    sensor.prec = 2;
    sensor.custom.ratio = 1234;
    sensor.custom.offset = -1234;
    sensor.logs = 1;
    sensor.filter = 1;
    sensor.autoOffset = 1;
    sensor.onlyPositive = 1;
    sensor.persistent = 1;
  }
  EXPECT_STREQ("configuration file too large", saveModelConfig("/MODELS/test.yml"));
  EXPECT_EQ(original, get());
  EXPECT_FALSE(std::filesystem::exists(root / "MODELS/test.yml.tmp"));
}
}
#if defined(STORAGE_MODELSLIST)
namespace {
TEST_F(ModelConfigFile, LabelEditPreservesCandidateSources)
{
  strcpy(g_model.header.name, "Active");
  g_model.timers[0].start = 999;
  put("header:\n  name: Other\nmixData:\n  0:\n    srcRaw: ch(3)\n    weight: 100\n");
  ASSERT_EQ(nullptr, saveModelConfigLabels("/MODELS/test.yml", "new"));
  EXPECT_STREQ("Active", g_model.header.name);
  EXPECT_EQ(999u, g_model.timers[0].start);
  EXPECT_NE(std::string::npos, get().find("srcRaw:"));
  ASSERT_EQ(nullptr, loadModelConfig("/MODELS/test.yml", g_model));
  EXPECT_STREQ("new", g_model.header.labels);
  EXPECT_EQ(MIXSRC_FIRST_CH + 3, g_model.mixData[0].srcRaw);
}
}
#endif
#if defined(COLORLCD)
namespace {
TEST_F(ModelConfig, EmptyAllocatedScreensAndOptionsAreOmitted)
{
  g_model.resetScreenData();
  g_model = ModelData{};
  g_model.rfAlarms.warning = 45; g_model.rfAlarms.critical = 42;
  g_model.getScreenData(0); // Allocation alone is not a used screen.
  ASSERT_TRUE(save(g_model));
  EXPECT_EQ(std::string::npos, output.find("screenData:"));
  g_model.setScreenLayoutId(3, "Layout1x1");
  ASSERT_TRUE(save(g_model));
  EXPECT_NE(std::string::npos, output.find("screenData:\n  3:"));
  EXPECT_EQ(std::string::npos, output.find("options:"));
  input = output;
  ASSERT_TRUE(load(g_model));
  EXPECT_STREQ("Layout1x1", g_model.getScreenLayoutId(3));
  auto& zone = g_model.getScreenData(3)->layoutData.zones[0];
  zone.widgetName = "Zero";
  zone.widgetData.options.resize(2);
  zone.widgetData.options[0].type = WOV_Color;
  zone.widgetData.options[0].value.unsignedValue = 0;
  zone.widgetData.options[1].type = WOV_Source;
  zone.widgetData.options[1].value.unsignedValue = 0;
  ASSERT_TRUE(save(g_model));
  EXPECT_EQ(std::string::npos, output.find("COLIDX0"));
  EXPECT_EQ(std::string::npos, output.find("source: \"NONE\""));
  input = output;
  ASSERT_TRUE(load(g_model));
  auto& options = g_model.getWidgetData(3, 0)->options;
  ASSERT_EQ(2u, options.size());
  EXPECT_EQ(WOV_Color, options[0].type);
  EXPECT_EQ(0u, options[0].value.unsignedValue);
  EXPECT_EQ(WOV_Source, options[1].type);
  EXPECT_EQ(0u, options[1].value.unsignedValue);
  g_model.resetScreenData();
}
}
#endif
