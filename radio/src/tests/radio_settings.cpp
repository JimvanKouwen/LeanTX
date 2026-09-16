// LeanTX radio configuration integration tests. GPL-2.0-or-later.
#include "gtests.h"
#include "storage/radio_config_adapter.h"
#include "storage/radio_config_file.h"
#include "storage/storage.h"
#include "location.h"
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
  EXPECT_NE(0, storageDirtyMsk & EE_GENERAL);
  ASSERT_EQ(nullptr, writeGeneralSettings());
  EXPECT_NE(std::string::npos, get().find("  nested: [one, two]"));
  storageDirtyMsk = 0;
  ASSERT_EQ(nullptr, loadRadioSettings()); EXPECT_EQ(0, storageDirtyMsk & EE_GENERAL);
}
TEST_F(RadioSettings, MalformedOriginalSurvives) {
  put("stickMode: 3\nfuture: [broken\n"); auto original = get();
  ASSERT_NE(nullptr, loadRadioSettings()); EXPECT_EQ(0, g_eeGeneral.stickMode);
  ASSERT_NE(nullptr, writeGeneralSettings()); EXPECT_EQ(original, get());
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
TEST_F(RadioSettings, AllAvailableDefaultsAreValid) {
  auto schema = radioSettingsSchema();
  for (unsigned i = 0; i < schema.count; ++i) {
    radio_config::Field field{};
    if (schema.field(schema.context, i, field) && field.available) {
      EXPECT_TRUE(schema.set(schema.context, i, field.value)) << field.path << ": " << field.value;
    }
  }
}
TEST_F(RadioSettings, FailedTemporaryWritePreservesOriginal) {
  put("stickMode: 2\nfuture: preserve\n"); auto original = get();
  simuFatfsSetWriteBudget(100);
  ASSERT_NE(nullptr, writeGeneralSettings()); EXPECT_EQ(original, get());
  EXPECT_FALSE(std::filesystem::exists(root / "RADIO/radio.yml.swap"));
}
TEST_F(RadioSettings, RemovedSettingsPreserved) {
  const std::string obsolete = "customFn:\n  0:\n    swtch: ON\n    func: OVERRIDE_CHANNEL\n    def: 0,100,1\n";
  put(obsolete + "speakerVolume: 3\n");
  ASSERT_EQ(nullptr, loadRadioSettings()); EXPECT_EQ(3 - VOLUME_LEVEL_DEF, g_eeGeneral.speakerVolume);
  ASSERT_EQ(nullptr, writeGeneralSettings()); EXPECT_NE(std::string::npos, get().find(obsolete));
}

}
namespace {
TEST_F(RadioSettings, InvalidStringDoesNotBecomeACollectionName) {
  put("uiLanguage: [fr]\nsticksConfig:\n  0:\n    name: [bad]\n");
  ASSERT_EQ(nullptr, loadRadioSettings());
  EXPECT_EQ('e', g_eeGeneral.uiLanguage[0]);
  EXPECT_NE(0, storageDirtyMsk & EE_GENERAL);
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
  EXPECT_NE(std::string::npos, get().find("imuMax: 14"));
#if defined(COLORLCD)
  EXPECT_NE(std::string::npos, get().find("blOffBright: 9"));
#else
  EXPECT_NE(std::string::npos, get().find("blOffBright: 7"));
  EXPECT_NE(std::string::npos, get().find("selectedTheme: 'portable'"));
#endif
  EXPECT_NE(std::string::npos, get().find("  - nested: [one, two]"));
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
TEST_F(RadioSettings, EveryBoundFieldHasADirectLookup) {
  auto schema = radioSettingsSchema();
  for (unsigned i = 0; i < schema.count; ++i) {
    radio_config::Field field{};
    if (schema.field(schema.context, i, field)) {
      EXPECT_EQ(int(i), schema.resolve(schema.context, field.path)) << field.path;
    }
  }
}
}
