// Reproducible host/FatFs storage benchmark. GPL-2.0-or-later.
#include "gtests.h"
#include "location.h"
#include "storage/storage.h"
#include "storage/model_config_file.h"
#include "storage/radio_config_file.h"
#include "storage/config_file_workspace.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <unistd.h>
TEST(ConfigBenchmark, FileTransactions) {
  char path[] = "/tmp/leantx-benchmark-XXXXXX";
  std::filesystem::path root = mkdtemp(path);
  std::filesystem::create_directory(root / "RADIO");
  std::filesystem::create_directory(root / "MODELS");
  simuFatfsSetPaths(root.c_str(), nullptr);
  generalDefault(); postRadioSettingsLoad();
  g_model = ModelData{};
  g_model.rfAlarms.warning = 45; g_model.rfAlarms.critical = 42;
  strcpy(g_model.header.name, "FourCh");
  for (unsigned i = 0; i < 4; ++i) {
    g_model.mixData[i].srcRaw = MIXSRC_FIRST_INPUT + i;
    g_model.mixData[i].destCh = i; g_model.mixData[i].weight = 100;
    g_model.expoData[i].srcRaw = MIXSRC_FIRST_STICK + i;
    g_model.expoData[i].chn = i;
    g_model.expoData[i].weight = 100;
  }
  ASSERT_EQ(nullptr, writeGeneralSettings());
  ASSERT_EQ(nullptr, saveModelConfig("/MODELS/bench.yml"));
  auto measure = [](auto action) {
    auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < 10; ++i) EXPECT_EQ(nullptr, action());
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 10;
  };
  const double rl = measure([] { return loadRadioSettingsYaml(true); });
  const double rs = measure([] { return writeGeneralSettings(); });
  const double ml = measure([] { return loadModelConfig("/MODELS/bench.yml", g_model); });
  const double ms = measure([] { return saveModelConfig("/MODELS/bench.yml"); });
  printf("BENCH radio_bytes=%zu model_bytes=%zu workspace=%zu RadioData=%zu ModelData=%zu radio_load_us=%.1f radio_save_us=%.1f model_load_us=%.1f model_save_us=%.1f\n",
    size_t(std::filesystem::file_size(root / "RADIO/radio.yml")), size_t(std::filesystem::file_size(root / "MODELS/bench.yml")),
    sizeof(config_file::Workspace), sizeof(RadioData), sizeof(ModelData), rl, rs, ml, ms);
  simuFatfsSetPaths(TESTS_PATH, nullptr);
  std::filesystem::remove_all(root);
}
