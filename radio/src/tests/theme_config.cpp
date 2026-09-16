// Theme storage uses semantic fields, never packed object offsets.
// GPL-2.0-or-later.
#include "gtests.h"
#if defined(COLORLCD)
#include "theme_config.h"
#include "location.h"
#include <filesystem>
#include <fstream>

class ThemeConfigFile : public testing::Test {
 protected:
  std::filesystem::path root;
  void SetUp() override {
    char path[] = "/tmp/leantx-theme-XXXXXX";
    root = mkdtemp(path);
    simuFatfsSetPaths(root.c_str(), nullptr);
  }
  void TearDown() override {
    simuFatfsSetWriteBudget(-1);
    simuFatfsFailRenameAfter(-1);
    simuFatfsSetPaths(TESTS_PATH, nullptr);
    std::filesystem::remove_all(root);
  }
  void put(const char* text) { std::ofstream(root / "theme.yml") << text; }
  std::string get() {
    std::ifstream file(root / "theme.yml");
    return std::string(std::istreambuf_iterator<char>(file), {});
  }
};
TEST_F(ThemeConfigFile, LegacyRgbHexDefaultsAndEscapedSummaryRoundTrip)
{
  put("---\nsummary:\n  name: Legacy\n  author: 'A \"quoted\" author'\n"
      "  info: \"line\\nnext\"\ncolors:\n  PRIMARY1: RGB(255,0,128)\n"
      "  PRIMARY2: 0x0102FF\nfuture:\n  extension: yes\n");
  ThemeConfig theme;
  for (auto& color : theme.colors) color = RGB(22, 33, 44);
  ASSERT_EQ(nullptr, loadThemeConfig("/theme.yml", theme));
  EXPECT_STREQ("Legacy", theme.name);
  EXPECT_STREQ("A \"quoted\" author", theme.author);
  EXPECT_STREQ("line\nnext", theme.info);
  EXPECT_EQ(RGB(255, 0, 128), theme.colors[0]);
  EXPECT_EQ(RGB(1, 2, 255), theme.colors[1]);
  EXPECT_EQ(RGB(22, 33, 44), theme.colors[2]);
  ASSERT_EQ(nullptr, saveThemeConfig("/theme.yml", theme));
  EXPECT_EQ(0u, get().find("---\n"));
  ThemeConfig again;
  ASSERT_EQ(nullptr, loadThemeConfig("/theme.yml", again));
  EXPECT_STREQ(theme.name, again.name);
  EXPECT_STREQ(theme.author, again.author);
  EXPECT_STREQ(theme.info, again.info);
  for (unsigned i = 0; i < THEME_COLOR_COUNT - 1; ++i)
    EXPECT_EQ(theme.colors[i], again.colors[i]);
}
TEST_F(ThemeConfigFile, MalformedLoadDoesNotCommit)
{
  for (auto yaml : {"summary:\n  name: Partial\nbad: [\n",
                    "colors:\n  PRIMARY1: RGB(300,0,0)\n",
                    "colors:\n  PRIMARY1: 0xFFFFFFF\n",
                    "colors:\n  PRIMARY1: 0x123456junk\n"}) {
    put(yaml);
    ThemeConfig theme;
    strcpy(theme.name, "Original");
    theme.colors[0] = RGB(22, 33, 44);
    EXPECT_NE(nullptr, loadThemeConfig("/theme.yml", theme));
    EXPECT_STREQ("Original", theme.name);
    EXPECT_EQ(RGB(22, 33, 44), theme.colors[0]);
  }
}
TEST_F(ThemeConfigFile, FailedWritesAndRenameRetainOriginal)
{
  put("summary:\n  name: Original\n");
  const auto original = get();
  ThemeConfig theme;
  strcpy(theme.name, "Replacement");
  simuFatfsSetWriteBudget(20);
  EXPECT_NE(nullptr, saveThemeConfig("/theme.yml", theme));
  EXPECT_EQ(original, get());
  simuFatfsSetWriteBudget(-1);
  simuFatfsFailRenameAfter(1);
  EXPECT_NE(nullptr, saveThemeConfig("/theme.yml", theme));
  EXPECT_EQ(original, get());
  simuFatfsFailRenameAfter(-1);
  EXPECT_EQ(nullptr, saveThemeConfig("/theme.yml", theme));
}
#endif
