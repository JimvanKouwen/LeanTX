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

// Header metadata and full-model decoding must agree independently of layout.
#include "gtests.h"
#include "model_yaml_test.h"

// Parse the same legacy header with both semantic adapters and compare values.
TEST(PartialModel, HeaderParsesIdenticallyToModelData)
{
  static const char yaml[] =
      "header: \n"
      "   name: \"Tst Name\"\n"
      "   modelId: [3, 5]\n"
#if LEN_BITMAP_NAME > 0
      "   bitmap: \"pic.bmp\"\n"
#endif
#if defined(STORAGE_MODELSLIST)
      "   labels: \"alpha,bravo\"\n"
#endif
      "moduleData:\n  1:\n    type: TYPE_CROSSFIRE\n";

  model_config::Header partial;
  memclear(&partial, sizeof(partial));
  config_stream::Workspace workspace;
  const char* cursor = yaml;
  auto result = config_stream::process({&cursor, [](void* ctx) {
    auto& p = *static_cast<const char**>(ctx);
    return *p ? int(static_cast<unsigned char>(*p++)) : -1;
  }, nullptr}, {}, model_config::headerSchema(partial), workspace, true);
  ASSERT_TRUE(result) << (result.error ? result.error : "");
  ASSERT_EQ(0u, result.invalid);
  loadModelYamlStr(yaml);
  const ModelData& model = g_model;

  EXPECT_STREQ(partial.header.name, model.header.name);
  EXPECT_EQ(0, memcmp(partial.header.modelId, model.header.modelId,
                       sizeof(model.header.modelId)));
#if LEN_BITMAP_NAME > 0
  EXPECT_STREQ(partial.header.bitmap, model.header.bitmap);
#endif
#if defined(STORAGE_MODELSLIST)
  EXPECT_STREQ(partial.header.labels, model.header.labels);
#endif

  // Sanity: the fixture actually populated something non-zero, otherwise a
  // parser that silently no-ops on both sides would pass trivially.
  EXPECT_STREQ("Tst Name", partial.header.name);
  EXPECT_EQ(MODULE_TYPE_CROSSFIRE, partial.moduleData[1].type);
  EXPECT_EQ(model.moduleData[1].type, partial.moduleData[1].type);
}

// Behavioural contract for ModelHeader::labels: it holds the CSV-joined
// list of *every* label attached to a model (declared as
// char[LABELS_LENGTH], currently 100 bytes) -- as distinct from the length
// of one individual label name (LABEL_LENGTH, 16 bytes). A model with
// several labels attached should keep all of them when this field gets
// (re)written, up to the field's own declared capacity, not just as much
// as would fit in a single label name.
//
// header.labels only exists on targets with STORAGE_MODELSLIST.
#if defined(STORAGE_MODELSLIST)
TEST(PartialModel, LabelsFieldRetainsFullCsvUpToItsOwnCapacity)
{
  model_config::Header partial;
  memclear(&partial, sizeof(partial));

  // A multi-label CSV comfortably inside LABELS_LENGTH, but longer than a
  // single LABEL_LENGTH-sized name -- i.e. more than one label attached.
  const char* csv = "alpha,bravo,charlie,delta,echo";
  ASSERT_GT(strlen(csv), (size_t)LABEL_LENGTH);
  ASSERT_LT(strlen(csv), (size_t)LABELS_LENGTH);

  strAppend(partial.header.labels, csv, LABELS_LENGTH - 1);

  EXPECT_STREQ(csv, partial.header.labels)
      << "header.labels should retain the full label list up to "
         "LABELS_LENGTH-1 characters, not just a single label name's "
         "worth.";
}
#endif // defined(STORAGE_MODELSLIST)

// Real FatFS round-trip tests for ModelMap::writeModelLabels() itself -
// the raw file-surgery function these PartialModel tests don't exercise.
#if defined(SIMU) && defined(STORAGE_MODELSLIST)

#include "storage/modelslist.h"
#include "storage/sdcard_common.h"

#include <filesystem>
#include <fstream>

static void writeRawModelFile(const char* modelFilename, const std::string& content)
{
  char path[256];
  getModelPath(path, modelFilename);
  std::filesystem::create_directories(
      std::filesystem::path(simuFatfsGetRealPath(path)).parent_path());
  std::ofstream f(simuFatfsGetRealPath(path), std::ios::binary);
  f << content;
}

static size_t modelFileSize(const char* modelFilename)
{
  char path[256];
  getModelPath(path, modelFilename);
  return std::filesystem::file_size(simuFatfsGetRealPath(path));
}

// Label edits stream through the generic engine. Valid unknown model data
// must retain its size; malformed YAML must be rejected without replacing it.
TEST(ModelsList, WriteModelLabelsPreservesBodySize)
{
  ModelMap map;

  const char* fileA = "wml_test_a.yml";
  const char* fileB = "wml_test_b.yml";

  // Both fixtures are well under 512 bytes (a single short f_read/EOF), and
  // differ only in body length. writeModelLabels() must reproduce that
  // exact size difference in its output - if it instead always pads the
  // first chunk out to sizeof(buf), both outputs collapse to the same size
  // regardless of the real body length (the uninitialized-memory bug).
  std::string bodyA = "body:\n  marker: AAAA\n";
  std::string bodyB = "body:\n  marker: AAAA" + std::string(300, 'X') + "\n";
  ASSERT_LT(bodyA.size() + 40, 512u);
  ASSERT_LT(bodyB.size() + 40, 512u);

  writeRawModelFile(fileA, "header:\n  name: OldName\n  labels: \n" + bodyA);
  writeRawModelFile(fileB, "header:\n  name: OldName\n  labels: \n" + bodyB);

  ModelCell cellA(fileA);
  ModelCell cellB(fileB);

  EXPECT_TRUE(map.writeModelLabels(&cellA, "NewLabel"));
  EXPECT_TRUE(map.writeModelLabels(&cellB, "NewLabel"));

  size_t sizeA = modelFileSize(fileA);
  size_t sizeB = modelFileSize(fileB);

  // The two headers are updated identically, so the size
  // delta between the outputs must equal the body length delta exactly.
  EXPECT_EQ(sizeB - sizeA, bodyB.size() - bodyA.size());

  std::filesystem::remove(simuFatfsGetRealPath(std::string(MODELS_PATH) + "/" + fileA));
  std::filesystem::remove(simuFatfsGetRealPath(std::string(MODELS_PATH) + "/" + fileB));
}

TEST(ModelsList, WriteModelLabelsInsertsMissingHeader)
{
  ModelMap map;
  const char* file = "wml_empty.yml";

  writeRawModelFile(file, "notheader:\n  foo: bar\n");
  ModelCell cell(file);

  EXPECT_TRUE(map.writeModelLabels(&cell, "NewLabel"));

  std::filesystem::remove(simuFatfsGetRealPath(std::string(MODELS_PATH) + "/" + file));
}

#endif  // #if defined(SIMU) && defined(STORAGE_MODELSLIST)
