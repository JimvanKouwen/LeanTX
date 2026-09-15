/*
 * Copyright (C) EdgeTX
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

#include "storage/radio_config.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "dataconstants.h"
#include "edgetx_constants.h"
#include "gtests.h"
#include "hal/switch_driver.h"
#include "location.h"
#include "sdcard.h"
#include "storage/storage.h"

namespace fs = std::filesystem;

class RadioConfigTest : public ::testing::Test
{
 protected:
  fs::path scratchDir;
  std::string radioPath;
  std::string tmpPath;

  void SetUp() override
  {
    scratchDir =
        fs::temp_directory_path() / fs::path("leantx-gtest-radio-config");
    std::error_code ec;
    fs::remove_all(scratchDir, ec);
    fs::create_directories(scratchDir / "RADIO", ec);
    ASSERT_FALSE(ec);

    simuFatfsSetPaths(scratchDir.string().c_str(), nullptr);

    radioPath = RADIO_SETTINGS_YAML_PATH;
    tmpPath = RADIO_SETTINGS_TMPFILE_YAML_PATH;

    memclear(&g_eeGeneral, sizeof(g_eeGeneral));
  }

  void TearDown() override
  {
    simuFatfsSetPaths(TESTS_PATH, nullptr);
    std::error_code ec;
    fs::remove_all(scratchDir, ec);
  }

  std::string realPath(const char* p) { return scratchDir.string() + p; }

  void writeRaw(const std::string& contents)
  {
    std::ofstream f(realPath(RADIO_SETTINGS_YAML_PATH), std::ios::binary);
    f << contents;
  }

  std::string readRaw()
  {
    std::ifstream f(realPath(RADIO_SETTINGS_YAML_PATH), std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
  }

  bool fileExists(const char* p) { return fs::exists(realPath(p)); }
};

TEST_F(RadioConfigTest, MissingFileLoadsDefaultsAndMarksDirty)
{
  g_eeGeneral.vBatWarn = 42;
  RadioConfigLoadResult r = radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  EXPECT_TRUE(r.fileMissing);
  EXPECT_TRUE(r.dirty);
  EXPECT_EQ(g_eeGeneral.vBatWarn, 42);  // unchanged - default already in memory
}

TEST_F(RadioConfigTest, NormalLoadAppliesKnownAvailableValues)
{
  writeRaw("version: 1\r\nvBatWarn: 90\r\nbacklightMode: 2\r\n");
  RadioConfigLoadResult r = radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  EXPECT_FALSE(r.fileMissing);
  EXPECT_EQ(g_eeGeneral.vBatWarn, 90);
  EXPECT_EQ(g_eeGeneral.backlightMode, 2);
}

TEST_F(RadioConfigTest, NormalSaveWritesCurrentRuntimeValues)
{
  g_eeGeneral.vBatWarn = 77;
  g_eeGeneral.backlightMode = 3;
  const char* err = radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                                    RADIO_SETTINGS_TMPFILE_YAML_PATH);
  EXPECT_EQ(err, nullptr);
  std::string data = readRaw();
  EXPECT_NE(data.find("vBatWarn: 77"), std::string::npos);
  EXPECT_NE(data.find("backlightMode: 3"), std::string::npos);
  EXPECT_FALSE(fileExists(RADIO_SETTINGS_TMPFILE_YAML_PATH));
}

TEST_F(RadioConfigTest, StableRoundTrip)
{
  g_eeGeneral.vBatWarn = 55;
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  std::string first = readRaw();

  memclear(&g_eeGeneral, sizeof(g_eeGeneral));
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  EXPECT_EQ(g_eeGeneral.vBatWarn, 55);

  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  std::string second = readRaw();
  EXPECT_EQ(first, second);
}

TEST_F(RadioConfigTest, MissingKnownFieldGetsDefault)
{
  g_eeGeneral.backlightMode = 4;  // "default" already in memory before load
  writeRaw("version: 1\r\nvBatWarn: 10\r\n");  // backlightMode absent
  RadioConfigLoadResult r = radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  EXPECT_GE(r.missingFields, 1);
  EXPECT_TRUE(r.dirty);
  EXPECT_EQ(g_eeGeneral.backlightMode, 4);  // default preserved
}

TEST_F(RadioConfigTest, MissingFieldAddedOnSave)
{
  writeRaw("version: 1\r\nvBatWarn: 10\r\n");
  g_eeGeneral.backlightMode = 5;
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  std::string data = readRaw();
  EXPECT_NE(data.find("backlightMode: 5"), std::string::npos);
}

TEST_F(RadioConfigTest, UnknownScalarSurvivesSave)
{
  writeRaw("version: 1\r\nvBatWarn: 10\r\nfutureSetting: 99\r\n");
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  EXPECT_NE(readRaw().find("futureSetting: 99"), std::string::npos);
}

TEST_F(RadioConfigTest, UnknownNestedMapSurvivesSave)
{
  writeRaw(
      "version: 1\r\n"
      "vBatWarn: 10\r\n"
      "someFutureFeature:\r\n"
      "  enabled: true\r\n"
      "  level: 2\r\n");
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  std::string data = readRaw();
  EXPECT_NE(data.find("someFutureFeature:"), std::string::npos);
  EXPECT_NE(data.find("enabled: true"), std::string::npos);
  EXPECT_NE(data.find("level: 2"), std::string::npos);
}

// "switches" is a *known* nested field: each switch letter is individually
// known+available/unavailable depending on the hardware switch count, and
// unrecognised per-switch attributes (e.g. a custom "name") are preserved.
TEST_F(RadioConfigTest, SwitchTypeIsAppliedAndRoundTrips)
{
  writeRaw(
      "version: 1\r\n"
      "vBatWarn: 10\r\n"
      "switches:\r\n"
      "  SA:\r\n"
      "    type: 3pos\r\n"
      "    name: \"\"\r\n");
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  EXPECT_EQ(g_eeGeneral.switchType(0), SWITCH_3POS);
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  std::string data = readRaw();
  EXPECT_NE(data.find("switches:"), std::string::npos);
  EXPECT_NE(data.find("SA:"), std::string::npos);
  EXPECT_NE(data.find("type: 3pos"), std::string::npos);
  // unrecognised per-switch attribute preserved unchanged
  EXPECT_NE(data.find("name: \"\""), std::string::npos);
}

TEST_F(RadioConfigTest, SwitchBeyondHardwareCountIsUnavailableAndPreserved)
{
  int maxSw = switchGetMaxAllSwitches();
  ASSERT_LT(maxSw, MAX_SWITCHES)
      << "test requires a switch slot beyond hardware count";
  char key[3] = {'S', (char)('A' + maxSw), 0};
  char yaml[256];
  snprintf(
      yaml, sizeof(yaml),
      "version: 1\r\nvBatWarn: 10\r\nswitches:\r\n  %s:\r\n    type: 3pos\r\n",
      key);
  writeRaw(yaml);
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  std::string data = readRaw();
  EXPECT_NE(data.find(std::string(key) + ":"), std::string::npos);
  EXPECT_NE(data.find("type: 3pos"), std::string::npos);
}

TEST_F(RadioConfigTest, MissingSwitchesBlockIsAddedOnSave)
{
  writeRaw("version: 1\r\nvBatWarn: 10\r\n");
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  std::string data = readRaw();
  EXPECT_NE(data.find("switches:"), std::string::npos);
  EXPECT_NE(data.find("SA:"), std::string::npos);
}

TEST_F(RadioConfigTest, UnknownListSurvivesSave)
{
  writeRaw(
      "version: 1\r\n"
      "vBatWarn: 10\r\n"
      "favorites:\r\n"
      "  - one\r\n"
      "  - two\r\n");
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  std::string data = readRaw();
  EXPECT_NE(data.find("- one"), std::string::npos);
  EXPECT_NE(data.find("- two"), std::string::npos);
}

TEST_F(RadioConfigTest, DeeplyNestedUnknownDataSurvives)
{
  writeRaw(
      "version: 1\r\n"
      "vBatWarn: 10\r\n"
      "a:\r\n"
      "  b:\r\n"
      "    c:\r\n"
      "      d: 1\r\n"
      "      e:\r\n"
      "        - x\r\n"
      "        - y\r\n");
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  std::string data = readRaw();
  EXPECT_NE(data.find("d: 1"), std::string::npos);
  EXPECT_NE(data.find("- x"), std::string::npos);
  EXPECT_NE(data.find("- y"), std::string::npos);
}

#if !defined(IMU)
TEST_F(RadioConfigTest, KnownButUnavailableSettingSurvivesWithoutBeingApplied)
{
  writeRaw("version: 1\r\nvBatWarn: 10\r\nimuMax: 42\r\n");
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  // preserved verbatim, never touched/applied (no imuMax member on this target)
  EXPECT_NE(readRaw().find("imuMax: 42"), std::string::npos);
}
#endif

// Display-type capability is a second, independent axis from IMU presence:
// "contrast" (OLED-only) and "radioThemesDisabled" (COLORLCD-only) are
// mutually exclusive on real hardware, so exactly one of the pair below is
// exercised as "known+available" and the other as "known+unavailable" on
// any given build.
#if !OLED_SCREEN
TEST_F(RadioConfigTest, ContrastUnavailableOnNonOledSurvivesUnchanged)
{
  writeRaw("version: 1\r\nvBatWarn: 10\r\ncontrast: 30\r\n");
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  EXPECT_NE(readRaw().find("contrast: 30"), std::string::npos);
}
#endif

#if defined(COLORLCD)
TEST_F(RadioConfigTest, ColorLcdOnlyFieldIsAppliedAndSaved)
{
  writeRaw("version: 1\r\nvBatWarn: 10\r\nradioThemesDisabled: true\r\n");
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  EXPECT_EQ(g_eeGeneral.radioThemesDisabled, 1);
  g_eeGeneral.radioThemesDisabled = 0;
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  EXPECT_NE(readRaw().find("radioThemesDisabled: false"), std::string::npos);
}
#else
TEST_F(RadioConfigTest, ColorLcdOnlyFieldUnavailableSurvivesUnchanged)
{
  writeRaw("version: 1\r\nvBatWarn: 10\r\nradioThemesDisabled: true\r\n");
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  EXPECT_NE(readRaw().find("radioThemesDisabled: true"), std::string::npos);
}
#endif

TEST_F(RadioConfigTest, ChangedKnownValueUpdatesWhileUnknownSiblingsSurvive)
{
  writeRaw("version: 1\r\nvBatWarn: 10\r\nfutureSetting: keepme\r\n");
  radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  g_eeGeneral.vBatWarn = 200;
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  std::string data = readRaw();
  EXPECT_NE(data.find("vBatWarn: 200"), std::string::npos);
  EXPECT_NE(data.find("futureSetting: keepme"), std::string::npos);
}

TEST_F(RadioConfigTest, InvalidKnownFieldTypeIsHandledSafely)
{
  g_eeGeneral.vBatWarn = 33;
  writeRaw("version: 1\r\nvBatWarn: not-a-number\r\n");
  RadioConfigLoadResult r = radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  EXPECT_GE(r.invalidFields, 1);
  EXPECT_EQ(g_eeGeneral.vBatWarn, 33);  // kept default/previous, not corrupted
}

TEST_F(RadioConfigTest, InvalidKnownFieldRangeIsHandledSafely)
{
  g_eeGeneral.backlightMode = 1;
  writeRaw("version: 1\r\nbacklightMode: 999\r\n");
  RadioConfigLoadResult r = radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  EXPECT_GE(r.invalidFields, 1);
  EXPECT_EQ(g_eeGeneral.backlightMode, 1);
}

TEST_F(RadioConfigTest, MalformedYamlDoesNotDestroyOriginalFile)
{
  writeRaw("version: 1\r\nvBatWarn: 10\r\n");
  // Load must not crash and must not corrupt runtime/original file even
  // when fed garbage.
  writeRaw(":::: not yaml at all ][");
  RadioConfigLoadResult r = radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  (void)r;
  EXPECT_TRUE(fileExists(RADIO_SETTINGS_YAML_PATH));
}

TEST_F(RadioConfigTest, SameConfigLoadedOnDifferentCapabilitySetsIsSafe)
{
  // The IMU fields are always present in the schema regardless of target;
  // loading a file that contains them must never crash even when this
  // build has no IMU (they are simply known+unavailable here).
  writeRaw(
      "version: 1\r\nvBatWarn: 10\r\nimuMax: 5\r\nimuOffset: -5\r\nimuInvert: "
      "1\r\n");
  RadioConfigLoadResult r = radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  EXPECT_EQ(g_eeGeneral.vBatWarn, 10);
  (void)r;
}

TEST_F(RadioConfigTest, DirtyFlagOnlySetWhenSomethingChanged)
{
  // A file saved from the current runtime state already contains every
  // known+available field (and switch), so reloading it must not report
  // anything missing/dirty.
  ASSERT_EQ(radioConfigSave(RADIO_SETTINGS_YAML_PATH,
                            RADIO_SETTINGS_TMPFILE_YAML_PATH),
            nullptr);
  RadioConfigLoadResult r = radioConfigLoad(RADIO_SETTINGS_YAML_PATH);
  EXPECT_EQ(r.missingFields, 0u);
  EXPECT_FALSE(r.dirty);
}
