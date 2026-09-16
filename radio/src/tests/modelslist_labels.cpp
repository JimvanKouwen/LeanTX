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

// Custom-screen and topbar-widget layout data (CustomScreenData /
// TopBarPersistentData) is stored per-process in static globals rather than
// inside ModelData itself (see ModelData::getScreenData()/getTopbarData()),
// because that data can be large and only the currently loaded model's copy
// needs to be resident. Any code that reads or writes a *full* ModelData
// buffer -- as opposed to the header-only PartialModel -- for a model other
// than the one currently loaded therefore risks reading/writing those
// globals on behalf of the wrong model.
//
// Label management (renaming/adding/removing labels) routinely touches
// models other than the currently loaded one. These tests exercise that
// label-management path through its public ModelMap API and assert the
// currently loaded model's screen/topbar data is left alone, regardless of
// what label edits are made to other models on disk.

#include "gtests.h"
#include "storage/model_config_adapter.h"
#include "location.h"

#include <filesystem>
#include <fstream>

#include "storage/modelslist.h"
#include "storage/sdcard_common.h"
#include "storage/sdcard_yaml.h"
#include "model_yaml_test.h"

#if defined(COLORLCD)

namespace fs = std::filesystem;

class ModelMapFsTest : public ::testing::Test
{
 protected:
  fs::path scratchDir;

  void SetUp() override
  {
    scratchDir = fs::temp_directory_path() /
                 fs::path("edgetx-gtest-modelslist-labels");
    std::error_code ec;
    fs::remove_all(scratchDir, ec);
    fs::create_directories(scratchDir / "MODELS", ec);
    ASSERT_FALSE(ec) << "could not create scratch MODELS directory";

    simuFatfsSetPaths(scratchDir.string().c_str(), nullptr);

    modelslist.clear();
    modelslabels.clear();
    modelslabels.clearFilter();
    memclear(&g_model, sizeof(g_model));
  }

  void TearDown() override
  {
    modelslist.clear();
    modelslabels.clear();
    modelslabels.clearFilter();

    simuFatfsSetPaths(TESTS_PATH, nullptr);

    std::error_code ec;
    fs::remove_all(scratchDir, ec);
  }

  // Writes a standalone model YAML file to <scratchDir>/MODELS/<filename>
  // with the given label list and screen/topbar marker values. This must be
  // called *before* the active model's own screen/topbar markers are set on
  // g_model: since getScreenData()/getTopbarData() are shared globals, using
  // them here to build this "other" model's fixture is itself writing
  // through the same storage g_model uses, exactly like the code under test
  // does. The test bodies set the active model's markers afterwards, which
  // simulates that model actually being loaded, and is what the assertions
  // check survives the label edit untouched.
  void writeFixtureModel(const char* filename, const char* labels,
                          const char* screenLayoutId, const char* widgetName)
  {
    ModelData model;
    memclear(&model, sizeof(model));
    // A real model always has a name; an entirely-default header (no name,
    // no labels, zeroed modelId) is omitted from the written YAML altogether
    // as a compaction optimisation, which would leave this fixture without
    // a "header:" section at all. Giving it a name keeps the fixture
    // realistic and avoids that unrelated edge case.
    strAppend(model.header.name, filename, LEN_MODEL_NAME);
    strAppend(model.header.labels, labels, LABELS_LENGTH - 1);
    model.getScreenData(0)->LayoutId = screenLayoutId;
    model.getTopbarData()->zones[0].widgetName = widgetName;

    char path[256];
    getModelPath(path, filename);
    std::ofstream(scratchDir / "MODELS" / filename, std::ios::binary)
        << saveModelYamlStr(model);
  }
};

TEST_F(ModelMapFsTest, RenamingLabelOnOtherModelLeavesActiveScreenDataUntouched)
{
  // Seed a second, non-active model on disk with a label and screen/topbar
  // data distinct from the active model's.
  writeFixtureModel("model0002.yml", "Foo", "OtherLayout", "OtherWidget");
  ModelCell* other = modelslist.addModel("model0002.yml", false);
  ASSERT_NE(other, nullptr);
  ASSERT_FALSE(modelslabels.addLabelToModel("Foo", other, false));

  // Now "load" the active model: register its cell and mark it current, and
  // set its screen/topbar data to its own values.
  ModelCell* active = modelslist.addModel("model0001.yml", false);
  ASSERT_NE(active, nullptr);
  modelslist.setCurrentModel(active);
  g_model.getScreenData(0)->LayoutId = "ActiveLayout";
  g_model.getTopbarData()->zones[0].widgetName = "ActiveWidget";

  // Rename a label that only exists on the other, non-active model.
  modelslabels.renameLabel("Foo", "Bar");

  // The active model's screen/topbar data must be unaffected by editing an
  // unrelated model's labels.
  EXPECT_STREQ(g_model.getScreenData(0)->LayoutId.c_str(), "ActiveLayout");
  EXPECT_STREQ(g_model.getTopbarData()->zones[0].widgetName.c_str(),
               "ActiveWidget");

  // The other model's file on disk should reflect the renamed label. Read
  // back only the header (PartialModel), not a full ModelData, so this
  // verification step doesn't itself touch the shared screen/topbar globals.
  model_config::Header partial{};
  memclear(&partial, sizeof(partial));
  readModelHeaderYaml("model0002.yml", partial);
  EXPECT_STREQ(partial.header.labels, "Bar");
}

TEST_F(ModelMapFsTest,
       AddingLabelToOtherModelWithFileUpdateLeavesActiveScreenDataUntouched)
{
  writeFixtureModel("model0002.yml", "", "OtherLayout", "OtherWidget");
  ModelCell* other = modelslist.addModel("model0002.yml", false);
  ASSERT_NE(other, nullptr);

  ModelCell* active = modelslist.addModel("model0001.yml", false);
  ASSERT_NE(active, nullptr);
  modelslist.setCurrentModel(active);
  g_model.getScreenData(0)->LayoutId = "ActiveLayout";
  g_model.getTopbarData()->zones[0].widgetName = "ActiveWidget";

  // update=true drives ModelMap::updateModelFile(), the second call site
  // that reads/writes a non-active model's file on disk.
  EXPECT_FALSE(modelslabels.addLabelToModel("Baz", other, true));

  EXPECT_STREQ(g_model.getScreenData(0)->LayoutId.c_str(), "ActiveLayout");
  EXPECT_STREQ(g_model.getTopbarData()->zones[0].widgetName.c_str(),
               "ActiveWidget");

  model_config::Header partial{};
  memclear(&partial, sizeof(partial));
  readModelHeaderYaml("model0002.yml", partial);
  EXPECT_STREQ(partial.header.labels, "Baz");
}


TEST(ModelsList, LegacyFileHashHasExplicitByteOrder)
{
  FILINFO info{};
  info.fsize = 0x12345678;
  info.fdate = 0x9abc;
  info.ftime = 0xdef0;
  char hash[FILE_HASH_LENGTH + 1];
  EXPECT_STREQ("78563412bc9af0de", FILInfoToHexStr(hash, &info));
}

TEST_F(ModelMapFsTest, LabelsCacheRoundTripAndStartup)
{
  writeFixtureModel("model0002.yml", "Plane", "Layout", "Widget");
  strcpy(g_eeGeneral.currModelFilename, "model0002.yml");
  ASSERT_TRUE(modelslist.load());
  ASSERT_EQ(modelslist.size(), 1u);
  auto* cell = modelslist.at(0);
  ASSERT_EQ(modelslist.getCurrentModel(), cell);
  cell->lastOpened = 123456;
  cell->modelId[0] = 42;
  cell->moduleData[0].type = 3;
  cell->moduleData[0].subType = 2;
  modelslabels.addLabel("Empty");
  modelslabels.addFilteredLabel("Plane");
  modelslabels.setSortOrder(DATE_DES);
  ASSERT_EQ(modelslist.save(), nullptr);
  modelslist.clear();
  modelslabels.clear();
  modelslabels.clearFilter();
  modelslabels.setSortOrder(NO_SORT);
  ASSERT_TRUE(modelslist.load());
  ASSERT_EQ(modelslist.size(), 1u);
  cell = modelslist.at(0);
  EXPECT_EQ(modelslist.getCurrentModel(), cell);
  EXPECT_EQ(cell->lastOpened, 123456);
  EXPECT_EQ(cell->modelId[0], 42);
  EXPECT_EQ(cell->moduleData[0].type, 3);
  EXPECT_EQ(cell->moduleData[0].subType, 2);
  EXPECT_FALSE(cell->_isDirty);
  EXPECT_TRUE(modelslabels.isLabelSelected("Plane", cell));
  EXPECT_TRUE(modelslabels.isLabelFiltered("Plane"));
  EXPECT_EQ(modelslabels.sortOrder(), DATE_DES);
  EXPECT_EQ(modelslabels.getLabels(), (LabelsVector{"Plane", "Empty"}));
}

TEST_F(ModelMapFsTest, LegacyCaseSelectionAndStaleCache)
{
  writeFixtureModel("model0002.yml", "Actual", "Layout", "Widget");
  std::ofstream(scratchDir / "MODELS/labels.yml") <<
      "lAbElS:\n  Empty:\n  Plane:\n    SeLeCtEd: false\n"
      "sOrT: 2\nModels:\n  missing.yml:\n    hash: bad\n"
      "  model0002.yml:\n    hash: stale\n    name: Wrong\n"
      "    labels: Wrong\n    lastopen: 0x123\n";
  strcpy(g_eeGeneral.currModelFilename, "model0002.yml");
  ASSERT_TRUE(modelslist.load());
  ASSERT_EQ(modelslist.size(), 1u);
  EXPECT_EQ(modelslist.getCurrentModel(), modelslist.at(0));
  EXPECT_STREQ(g_eeGeneral.currModelFilename, "model0002.yml");
  EXPECT_EQ(modelslist.at(0)->lastOpened, 0x123);
  EXPECT_TRUE(modelslabels.isLabelFiltered("Plane"));
  EXPECT_TRUE(modelslabels.isLabelSelected("Actual", modelslist.at(0)));
  EXPECT_FALSE(modelslabels.isLabelSelected("Wrong", modelslist.at(0)));
  EXPECT_EQ(modelslabels.sortOrder(), NAME_DES);
}

TEST_F(ModelMapFsTest, StartupFallsBackToExistingModelAndLoadsOnlyOnce)
{
  writeFixtureModel("model0002.yml", "Actual", "Layout", "Widget");
  strcpy(g_eeGeneral.currModelFilename, "missing.yml");
  ASSERT_TRUE(modelslist.load());
  ASSERT_EQ(modelslist.size(), 1u);
  auto* current = modelslist.getCurrentModel();
  EXPECT_EQ(current, modelslist.at(0));
  EXPECT_STREQ(g_eeGeneral.currModelFilename, "model0002.yml");
  ASSERT_TRUE(modelslist.load());
  EXPECT_EQ(modelslist.getCurrentModel(), current);
  EXPECT_EQ(modelslist.size(), 1u);
}

TEST_F(ModelMapFsTest, LabelsStreamHasNoFixedSchemaEntryLimit)
{
  writeFixtureModel("model0002.yml", "", "Layout", "Widget");
  {
    std::ofstream file(scratchDir / "MODELS/labels.yml");
    file << "Labels:\n";
    for (int i = 0; i < 1100; ++i) file << "  Label" << i << ":\n";
    file << "Sort: 0\n";
  }
  ASSERT_TRUE(modelslist.load());
  EXPECT_EQ(modelslabels.getLabels().size(), 1100u);
  EXPECT_EQ(modelslabels.getLabels().back(), "Label1099");
  EXPECT_EQ(modelslist.save(), nullptr);
}

TEST_F(ModelMapFsTest, MalformedLabelsDoNotApplyOrOverwrite)
{
  writeFixtureModel("model0002.yml", "Actual", "Layout", "Widget");
  const std::vector<std::string> documents = {
      "Labels:\n  Partial:\n    selected: true\nSort: [unterminated\n",
      "Labels:\n  " + std::string(1100, 'x') + ":\n",
      "Sort: 999\n",
      "Models:\n  model0002.yml:\n    bitmap: " + std::string(100, 'x') + "\n",
      "Models:\n  model0002.yml:\n    lastopen: garbage\n"};
  for (const auto& document : documents) {
    modelslist.clear();
    modelslabels.clear();
    modelslabels.clearFilter();
    std::ofstream(scratchDir / "MODELS/labels.yml") << document;
    ASSERT_TRUE(modelslist.load());
    ASSERT_EQ(modelslist.size(), 1u);
    EXPECT_TRUE(modelslabels.isLabelSelected("Actual", modelslist.at(0)));
    EXPECT_FALSE(modelslabels.isLabelFiltered("Partial"));
    EXPECT_NE(modelslist.save(), nullptr);
    std::ifstream file(scratchDir / "MODELS/labels.yml");
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(file), {}), document);
  }
}

#endif // defined(COLORLCD)
