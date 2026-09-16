// LeanTX radio configuration integration tests. GPL-2.0-or-later.
#include "gtests.h"
#include "storage/radio_config_adapter.h"
#include "storage/radio_config_file.h"
#include "storage/storage.h"
#include "location.h"
#include "analogs.h"
#include "hal/adc_driver.h"
#include "hal/switch_driver.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>
namespace {
class RadioSettings : public testing::Test {
 protected:
  std::filesystem::path root;
  void SetUp() override {
    char path[] = "/tmp/leantx-radio-config-XXXXXX";
    root = mkdtemp(path);
    std::filesystem::create_directory(root / "RADIO");
    simuFatfsSetPaths(root.string().c_str(), nullptr);
    // generalDefault initializes RadioData; simulate fresh-boot auxiliary state
    // too, so previous emergency/flex tests cannot leak mappings into defaults.
    for (int i = 0; i < MAX_FLEX_SWITCHES; ++i) switchConfigFlex_raw(i, -1);
    generalDefault(); postRadioSettingsLoad(); storageDirtyMsk = 0;
  }
  void TearDown() override {
    simuFatfsSetWriteBudget(-1);
    simuFatfsFailRenameAfter(-1);
    simuFatfsSetPaths(TESTS_PATH, nullptr);
    std::filesystem::remove_all(root);
    storageDirtyMsk = 0;
  }
  void put(const std::string& text) { std::ofstream(root / "RADIO/radio.yml") << text; }
  std::string get() {
    std::ifstream f(root / "RADIO/radio.yml");
    return std::string(std::istreambuf_iterator<char>(f), {});
  }
};
TEST_F(RadioSettings, DefaultsStableAndDirtyOnlyWhenNeeded) {
  ASSERT_EQ(nullptr, writeGeneralSettings());
  auto first = get();
  ASSERT_EQ(nullptr, loadRadioSettings());
  EXPECT_EQ(0, storageDirtyMsk & EE_GENERAL);
  ASSERT_EQ(nullptr, writeGeneralSettings()); EXPECT_EQ(first, get());
  put("stickMode: 2\nfuture:\n  nested: [one, two]\n");
  ASSERT_EQ(nullptr, loadRadioSettings()); EXPECT_EQ(2, g_eeGeneral.stickMode);
  EXPECT_EQ(0, storageDirtyMsk & EE_GENERAL);
  ASSERT_EQ(nullptr, writeGeneralSettings());
  EXPECT_EQ(std::string::npos, get().find("  nested: [one, two]"));
  storageDirtyMsk = 0;
  ASSERT_EQ(nullptr, loadRadioSettings()); EXPECT_EQ(0, storageDirtyMsk & EE_GENERAL);
}
TEST_F(RadioSettings, SaveRegeneratesWithoutReadingMalformedOriginal) {
  put("stickMode: 3\nfuture: [broken\n"); auto original = get();
  ASSERT_NE(nullptr, loadRadioSettings()); EXPECT_EQ(0, g_eeGeneral.stickMode);
  ASSERT_EQ(nullptr, writeGeneralSettings()); EXPECT_NE(original, get());
}
TEST_F(RadioSettings, FailedTemporaryOpenPreservesOriginal) {
  put("stickMode: 2\n"); auto original = get();
  std::filesystem::create_directory(root / "RADIO/radio.yml.tmp");
  ASSERT_NE(nullptr, writeGeneralSettings()); EXPECT_EQ(original, get());
}
TEST_F(RadioSettings, InterruptedCommitRecoversOriginal) {
  put("stickMode: 2\n");
  std::filesystem::rename(root / "RADIO/radio.yml", root / "RADIO/radio.yml.swap");
  std::ofstream(root / "RADIO/radio.yml.tmp") << "incomplete: [\n";
  ASSERT_EQ(nullptr, loadRadioSettings()); EXPECT_EQ(2, g_eeGeneral.stickMode);
  ASSERT_EQ(nullptr, writeGeneralSettings());
  EXPECT_FALSE(std::filesystem::exists(root / "RADIO/radio.yml.swap"));
}
TEST_F(RadioSettings, DebounceAndForcedFlush) {
  ASSERT_EQ(nullptr, writeGeneralSettings()); auto original = get();
  for (int i = 0; i < 30; ++i) {
    ++g_tmr10ms;
    g_eeGeneral.backlightBright = i;
    storageDirty(EE_GENERAL);
    EXPECT_FALSE(TIME_TO_WRITE()); EXPECT_EQ(original, get());
  }
  g_tmr10ms += WRITE_DELAY_10MS - 1; EXPECT_FALSE(TIME_TO_WRITE());
  ++g_tmr10ms; EXPECT_TRUE(TIME_TO_WRITE());
  // Existing forced flush entry point bypasses the caller's time gate.
  storageCheck(true); EXPECT_EQ(0, storageDirtyMsk & EE_GENERAL);
  EXPECT_NE(original, get());
}
TEST_F(RadioSettings, FailedTemporaryWritePreservesOriginal) {
  put("stickMode: 2\nfuture: preserve\n"); auto original = get();
  simuFatfsSetWriteBudget(1);
  ASSERT_NE(nullptr, writeGeneralSettings()); EXPECT_EQ(original, get());
  EXPECT_FALSE(std::filesystem::exists(root / "RADIO/radio.yml.swap"));
}
TEST_F(RadioSettings, RemovedSettingsDropped) {
  const std::string obsolete = "customFn:\n  0:\n    swtch: ON\n    func: OVERRIDE_CHANNEL\n    def: 0,100,1\n";
  put(obsolete + "speakerVolume: 3\n");
  ASSERT_EQ(nullptr, loadRadioSettings()); EXPECT_EQ(3 - VOLUME_LEVEL_DEF, g_eeGeneral.speakerVolume);
  ASSERT_EQ(nullptr, writeGeneralSettings()); EXPECT_EQ(std::string::npos, get().find(obsolete));
}

}
namespace {
TEST_F(RadioSettings, InvalidStringDoesNotBecomeACollectionName) {
  put("uiLanguage: [fr]\nsticksConfig:\n  0:\n    name: [bad]\n");
  ASSERT_NE(nullptr, loadRadioSettings());
  EXPECT_EQ('e', g_eeGeneral.uiLanguage[0]);
  EXPECT_EQ(0, storageDirtyMsk & EE_GENERAL);
}
TEST_F(RadioSettings, FutureVersionIsNotDowngraded) {
  put("configVersion: 12\nstickMode: 2\n");
  ASSERT_EQ(nullptr, loadRadioSettings());
  ASSERT_EQ(nullptr, writeGeneralSettings());
  EXPECT_NE(std::string::npos, get().find("configVersion: 12"));
}
}
namespace {
TEST_F(RadioSettings, SwitchAndInvertedAnalogSourcesRoundtrip) {
  g_eeGeneral.backlightSrc = MIXSRC_FIRST_SWITCH;
  g_eeGeneral.volumeSrc = -MIXSRC_FIRST_POT;
  ASSERT_TRUE(isSourceAvailableForBacklightOrVolume(g_eeGeneral.backlightSrc));
  ASSERT_TRUE(isSourceAvailableForBacklightOrVolume(g_eeGeneral.volumeSrc));
  ASSERT_EQ(nullptr, writeGeneralSettings());
  ASSERT_EQ(nullptr, loadRadioSettings());
  EXPECT_EQ(MIXSRC_FIRST_SWITCH, g_eeGeneral.backlightSrc);
  EXPECT_EQ(-MIXSRC_FIRST_POT, g_eeGeneral.volumeSrc);
}
TEST_F(RadioSettings, CrsfRuntimeEnumIsNotABoolean) {
#if defined(INTERNAL_MODULE_CRSF)
  g_eeGeneral.internalModule = MODULE_TYPE_CROSSFIRE;
  ASSERT_EQ(nullptr, writeGeneralSettings());
  ASSERT_EQ(nullptr, loadRadioSettings());
  EXPECT_EQ(MODULE_TYPE_CROSSFIRE, g_eeGeneral.internalModule);
  EXPECT_EQ(0, storageDirtyMsk & EE_GENERAL);
#endif
}

}
namespace {
TEST_F(RadioSettings, SameDocumentAcrossTargetCapabilities) {
  put("imuMax: 14\nblOffBright: 7\nselectedTheme: 'portable'\n"
      "calib:\n  LH:\n    mid: 1000\nfuture:\n  - nested: [one, two]\n");
  ASSERT_EQ(nullptr, loadRadioSettings());
  EXPECT_EQ(1000, g_eeGeneral.calib[0].mid);
#if defined(IMU)
  EXPECT_EQ(14, g_eeGeneral.imuMax);
#endif
#if defined(COLORLCD)
  EXPECT_EQ(7, g_eeGeneral.blOffBright);
  g_eeGeneral.blOffBright = 9;
#endif
  ASSERT_EQ(nullptr, writeGeneralSettings());
#if defined(IMU)
  EXPECT_NE(std::string::npos, get().find("imuMax: 14"));
#else
  EXPECT_EQ(std::string::npos, get().find("imuMax: 14"));
#endif
#if defined(COLORLCD)
  EXPECT_NE(std::string::npos, get().find("blOffBright: 9"));
#else
  EXPECT_EQ(std::string::npos, get().find("blOffBright: 7"));
  EXPECT_EQ(std::string::npos, get().find("selectedTheme: 'portable'"));
#endif
  EXPECT_EQ(std::string::npos, get().find("  - nested: [one, two]"));
}
}
namespace {
TEST_F(RadioSettings, LegacyAliasesImportWithoutBeingAddedToNewFiles) {
  put("telemetryBaudrate: 2\njitterFilter: 1\nrotEncDirection: 1\ncalib:\n  Rud:\n    mid: 1005\n");
  ASSERT_EQ(nullptr, loadRadioSettings());
  EXPECT_EQ(1005, g_eeGeneral.calib[0].mid);
  EXPECT_EQ(1, g_eeGeneral.noJitterFilter);
#if defined(INTERNAL_MODULE_CRSF)
  EXPECT_EQ(2, g_eeGeneral.internalModuleBaudrate);
#endif
  ASSERT_EQ(nullptr, writeGeneralSettings());
  auto merged = get();
  storageDirtyMsk = 0;
  ASSERT_EQ(nullptr, loadRadioSettings());
  EXPECT_EQ(0, storageDirtyMsk & EE_GENERAL);
  ASSERT_EQ(nullptr, writeGeneralSettings()); EXPECT_EQ(merged, get());
  std::filesystem::remove(root / "RADIO/radio.yml");
  ASSERT_EQ(nullptr, writeGeneralSettings());
  EXPECT_EQ(std::string::npos, get().find("telemetryBaudrate:"));
  EXPECT_EQ(std::string::npos, get().find("slidersConfig:"));
}
}

namespace {
TEST_F(RadioSettings, FailedCommitRenamesRestoreOriginal) {
  put("stickMode: 2\n"); auto original = get();
  for (int failure : {0, 1}) {
    simuFatfsFailRenameAfter(failure);
    ASSERT_NE(nullptr, writeGeneralSettings());
    EXPECT_EQ(original, get());
  }
  simuFatfsFailRenameAfter(-1);
  ASSERT_EQ(nullptr, writeGeneralSettings());
  EXPECT_FALSE(std::filesystem::exists(root / "RADIO/radio.yml.swap"));
}
}
namespace {

}

TEST_F(RadioSettings, SavedNumbersAreDecimal) {
  g_eeGeneral.globalTimer = UINT32_MAX;
  g_eeGeneral.txVoltageCalibration = -42;
  g_eeGeneral.stickMode = 0;
  g_eeGeneral.inactivityTimer = 123;
  ASSERT_EQ(nullptr, writeGeneralSettings());
  const auto saved = get();
  EXPECT_NE(std::string::npos, saved.find("globalTimer: 4294967295\n"));
  EXPECT_NE(std::string::npos, saved.find("txVoltageCalibration: -42\n"));
  EXPECT_EQ(std::string::npos, saved.find("stickMode: 0\n"));
  EXPECT_NE(std::string::npos, saved.find("inactivityTimer: 123\n"));
  EXPECT_EQ(std::string::npos, saved.find(": ld\n"));
  ASSERT_EQ(nullptr, loadRadioSettings());
  EXPECT_EQ(UINT32_MAX, g_eeGeneral.globalTimer);
  EXPECT_EQ(-42, g_eeGeneral.txVoltageCalibration);
  ASSERT_EQ(nullptr, writeGeneralSettings());
  EXPECT_EQ(saved, get());
}

TEST_F(RadioSettings, MalformedTailLeavesEveryLiveSettingUntouched) {
  put("configVersion: 17\nstickMode: 2\nsticksConfig:\n  0:\n    name: 'old'\n");
  ASSERT_EQ(nullptr, loadRadioSettingsYaml(true));
  const RadioData before = g_eeGeneral;
  const std::string label = analogGetCustomLabel(ADC_INPUT_MAIN, 0);
  storageDirtyMsk = 0;
  put("configVersion: 23\nstickMode: 3\nsticksConfig:\n  0:\n    name: 'new'\n" +
      std::string(400, '#') + "\nfuture: [broken\n");
  EXPECT_NE(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(0, memcmp(&before, &g_eeGeneral, sizeof(before)));
  EXPECT_EQ(label, analogGetCustomLabel(ADC_INPUT_MAIN, 0));
  EXPECT_EQ(0, storageDirtyMsk);
  std::filesystem::remove(root / "RADIO/radio.yml");
  ASSERT_EQ(nullptr, writeGeneralSettings());
  EXPECT_NE(std::string::npos, get().find("configVersion: 17"));
}

TEST_F(RadioSettings, CandidateRemainsPrivateUntilCommit) {
  g_eeGeneral.stickMode = 2;
  const RadioData before = g_eeGeneral;
  const std::string oldLabel = analogGetCustomLabel(ADC_INPUT_MAIN, 0);
  struct Input {
    const char* text;
    const RadioData* before;
    unsigned eof = 0;
    bool unchanged = true;
  } input{"stickMode: 3\nsticksConfig:\n  0:\n    name: 'new'\n", &before};
  auto schema = beginRadioSettingsLoad();
  config_stream::Workspace parser;
  auto read = [](void* ctx) -> int {
    auto& in = *static_cast<Input*>(ctx);
    in.unchanged &= !memcmp(in.before, &g_eeGeneral, sizeof(g_eeGeneral));
    if (!*in.text) { ++in.eof; return -1; }
    return (unsigned char)*in.text++;
  };
  auto result = config_stream::process({&input, read, nullptr}, {}, schema, parser, true);
  ASSERT_TRUE(result);
  EXPECT_EQ(1u, input.eof);
  EXPECT_TRUE(input.unchanged);
  EXPECT_EQ(oldLabel, analogGetCustomLabel(ADC_INPUT_MAIN, 0));
  resolveRadioSettingsLoad(result);
  EXPECT_EQ(0, memcmp(&before, &g_eeGeneral, sizeof(before)));
  EXPECT_EQ(oldLabel, analogGetCustomLabel(ADC_INPUT_MAIN, 0));
  commitRadioSettingsLoad();
  EXPECT_EQ(3, g_eeGeneral.stickMode);
  EXPECT_STREQ("new", analogGetCustomLabel(ADC_INPUT_MAIN, 0));
}

TEST_F(RadioSettings, CalibrationResolvesAfterHardwareRegardlessOfKeyOrder) {
#if XPOTS_MULTIPOS_COUNT > 0
  if (!adcGetMaxInputs(ADC_INPUT_FLEX)) GTEST_SKIP();
  const unsigned index = adcGetInputOffset(ADC_INPUT_FLEX);
  const std::string name = analogGetPhysicalName(ADC_INPUT_FLEX, 0);
  const std::string calibration = "calib:\n  " + name +
      ":\n    mid: bad\n    spanNeg: bad\n    spanPos: bad\n    count: 3\n"
      "    steps:\n      0: 20\n      1: 60\n      2: 100\n      3: 140\n      4: 180\n";
  const std::string hardware = "potsConfig:\n  " + name + ":\n    type: multipos_switch\n";
  for (bool hardwareFirst : {false, true}) {
    put(hardwareFirst ? hardware + calibration : calibration + hardware);
    ASSERT_EQ(nullptr, loadRadioSettingsYaml(true));
    const auto& steps = g_eeGeneral.calib[index].multipos;
    EXPECT_EQ(FLEX_MULTIPOS, getPotType(0));
    EXPECT_EQ(3, steps.count);
    EXPECT_EQ(20, steps.steps[0]);
    EXPECT_EQ(180, steps.steps[4]);
    ASSERT_EQ(nullptr, writeGeneralSettings());
    EXPECT_EQ(std::string::npos, get().find("mid: bad")); // unavailable, preserved verbatim
    storageDirtyMsk = 0;
    ASSERT_EQ(nullptr, loadRadioSettingsYaml(true));
    EXPECT_EQ(0, storageDirtyMsk & EE_GENERAL);
  }
  put("calib:\n  " + name + ":\n    mid: 777\n    spanNeg: 888\n    spanPos: 999\n    count: bad\n"
      "potsConfig:\n  " + name + ":\n    type: with_detent\n");
  ASSERT_EQ(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(777, g_eeGeneral.calib[index].mid);
  EXPECT_EQ(888, g_eeGeneral.calib[index].spanNeg);
  EXPECT_EQ(999, g_eeGeneral.calib[index].spanPos);
  ASSERT_EQ(nullptr, writeGeneralSettings());
  storageDirtyMsk = 0;
  ASSERT_EQ(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(0, storageDirtyMsk & EE_GENERAL);
#endif
}

TEST_F(RadioSettings, SourcesAndFlexAssignmentsResolveAfterHardware) {
  if (!adcGetMaxInputs(ADC_INPUT_FLEX)) GTEST_SKIP();
  const std::string name = analogGetPhysicalName(ADC_INPUT_FLEX, 0);
  put("volumeSrc: '!" + name + "'\npotsConfig:\n  " + name + ":\n    type: none\n");
  ASSERT_NE(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(0, g_eeGeneral.volumeSrc);
  put("volumeSrc: '!" + name + "'\npotsConfig:\n  " + name + ":\n    type: with_detent\n");
  ASSERT_EQ(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(-MIXSRC_FIRST_POT, g_eeGeneral.volumeSrc);
#if MAX_FLEX_SWITCHES > 0
  const std::string flexName = switchGetDefaultName(switchGetMaxSwitches());
  put("flexSwitches:\n  " + flexName + ":\n    channel: " + name +
      "\npotsConfig:\n  " + name + ":\n    type: switch\n");
  ASSERT_EQ(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(0, switchGetFlexConfig_raw(0));
  put("flexSwitches:\n  " + flexName + ":\n    channel: " + name +
      "\npotsConfig:\n  " + name + ":\n    type: switch\nfuture: [broken\n");
  EXPECT_NE(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(0, switchGetFlexConfig_raw(0));
#endif
}

TEST_F(RadioSettings, DuplicateFieldDiscardsCandidate) {
  g_eeGeneral.stickMode = 1;
  put("stickMode: 2\nstickMode: 3\n");
  EXPECT_NE(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(1, g_eeGeneral.stickMode);
}

TEST_F(RadioSettings, ReadFailureDiscardsCandidate) {
  g_eeGeneral.stickMode = 1;
  const RadioData before = g_eeGeneral;
  struct Input { const char* text; } input{"stickMode: 3\n"};
  auto schema = beginRadioSettingsLoad();
  config_stream::Workspace parser;
  auto read = [](void* ctx) -> int {
    auto& in = *static_cast<Input*>(ctx);
    return *in.text ? (unsigned char)*in.text++ : -2;
  };
  auto result = config_stream::process({&input, read, nullptr}, {}, schema, parser, true);
  EXPECT_FALSE(result);
  EXPECT_EQ(0, memcmp(&before, &g_eeGeneral, sizeof(before)));
  put("stickMode: 2\n");
  ASSERT_EQ(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(2, g_eeGeneral.stickMode);
}

TEST_F(RadioSettings, ToolNamesCommitOnlyAfterSuccessfulParse) {
#if defined(COLORLCD)
  ASSERT_TRUE(g_eeGeneral.configSetToolName(false, 0, "original"));
  put("keyShortcuts:\n  0:\n    shortcut: 'APP,candidate'\nfuture: [broken\n");
  EXPECT_NE(nullptr, loadRadioSettingsYaml(true));
  EXPECT_STREQ("original", g_eeGeneral.configToolName(false, 0));
  put("keyShortcuts:\n  0:\n    shortcut: 'APP,candidate'\n");
  ASSERT_EQ(nullptr, loadRadioSettingsYaml(true));
  EXPECT_STREQ("candidate", g_eeGeneral.configToolName(false, 0));
#endif
}

TEST_F(RadioSettings, SerialModesValidateAgainstCandidateNotLivePorts) {
  int first = -1, second = -1;
  for (unsigned port = 0; port < MAX_SERIAL_PORTS; ++port) {
    if (!serialGetPort(port)) continue;
    if (first < 0) first = port; else { second = port; break; }
  }
  if (second < 0) GTEST_SKIP();
  const char* ports[] = {"AUX1", "AUX2", "VCP"};
  serialSetMode(second, UART_MODE_TELEMETRY_MIRROR);
  put(std::string("serialPort:\n  ") + ports[first] + ":\n    mode: TELEMETRY_MIRROR\n");
  ASSERT_EQ(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(UART_MODE_TELEMETRY_MIRROR, serialGetMode(first));
  EXPECT_EQ(UART_MODE_NONE, serialGetMode(second));
  // Conflicting assignments invalidate the whole candidate.
  put(std::string("serialPort:\n  ") + ports[first] + ":\n    mode: TELEMETRY_MIRROR\n  " +
      ports[second] + ":\n    mode: TELEMETRY_MIRROR\n");
  ASSERT_NE(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(UART_MODE_TELEMETRY_MIRROR, serialGetMode(first));
  EXPECT_EQ(UART_MODE_NONE, serialGetMode(second));
  EXPECT_EQ(0, storageDirtyMsk & EE_GENERAL);
}

TEST_F(RadioSettings, UnavailableCalibrationContainerIsDropped) {
#if XPOTS_MULTIPOS_COUNT > 0
  if (!adcGetMaxInputs(ADC_INPUT_FLEX)) GTEST_SKIP();
  const std::string name = analogGetPhysicalName(ADC_INPUT_FLEX, 0);
  const std::string calibration = "calib:\n  " + name + ":\n    steps: [10, 20]\n";
  put(calibration + "potsConfig:\n  " + name + ":\n    type: with_detent\n");
  ASSERT_EQ(nullptr, loadRadioSettingsYaml(true));
  ASSERT_EQ(nullptr, writeGeneralSettings());
  EXPECT_EQ(std::string::npos, get().find("steps: [10, 20]"));
  const RadioData before = g_eeGeneral;
  put(calibration + "potsConfig:\n  " + name + ":\n    type: multipos_switch\n");
  EXPECT_NE(nullptr, loadRadioSettingsYaml(true));
  EXPECT_EQ(0, memcmp(&before, &g_eeGeneral, sizeof(before)));
#endif
}
