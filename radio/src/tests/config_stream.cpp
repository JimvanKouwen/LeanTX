// LeanTX streaming configuration tests. GPL-2.0-or-later.
#include "gtest/gtest.h"
#include "storage/config_stream.h"
#include <string>
#include <cstring>
using namespace config_stream;
namespace {
struct ConfigFixture : testing::Test {
  int brightness = 20, imu = 5, volume = 3;
  bool hasImu = false;
  std::string source, saved;
  size_t position = 0, failAt = size_t(-1);
  Workspace workspace;
  static bool field(void* ctx, unsigned i, Field& f) {
    auto& c = *static_cast<ConfigFixture*>(ctx);
    const char* paths[] = {"brightness", "imu", "audio/volume"};
    strcpy(f.path, paths[i]);
    snprintf(f.value, sizeof(f.value), "%d", i == 0 ? c.brightness : i == 1 ? c.imu : c.volume);
    f.available = i != 1 || c.hasImu; return true;
  }
  static bool set(void* ctx, unsigned i, const char* text) {
    auto& c = *static_cast<ConfigFixture*>(ctx); int64_t n;
    if (!integer(text, 0, 100, n)) return false;
    (i == 0 ? c.brightness : i == 1 ? c.imu : c.volume) = n;
    return true;
  }
  static bool known(void*, const char* path) {
    return !strcmp(path, "brightness") || !strcmp(path, "imu") || !strcmp(path, "audio") || !strcmp(path, "audio/volume");
  }
  static int read(void* ctx) {
    auto& c = *static_cast<ConfigFixture*>(ctx);
    return c.position == c.source.size() ? -1 : (unsigned char)c.source[c.position++];
  }
  static bool write(void* ctx, const char* text, size_t size) {
    auto& c = *static_cast<ConfigFixture*>(ctx);
    if (c.saved.size() + size > c.failAt) return false;
    c.saved.append(text, size); return true;
  }
  Result run(bool save = false) {
    position = 0; saved.clear();
    return process({this, read, nullptr}, {this, nullptr, save ? write : nullptr},
                   {3, this, field, set, known}, workspace, !save);
  }
};
TEST_F(ConfigFixture, Load) {
  source = "brightness: 42\naudio:\n  volume: 8\n";
  auto r = run(); ASSERT_TRUE(r); EXPECT_FALSE(r.missing);
  EXPECT_EQ(42, brightness); EXPECT_EQ(8, volume);
}
TEST_F(ConfigFixture, SaveRoundtrip) {
  source = "brightness: 42\naudio:\n  volume: 8\n";
  ASSERT_TRUE(run()); ASSERT_TRUE(run(true));
  auto first = saved; source = saved;
  ASSERT_TRUE(run()); EXPECT_FALSE(run().missing); ASSERT_TRUE(run(true)); EXPECT_EQ(first, saved);
}
TEST_F(ConfigFixture, MissingDefaultsAndInsertion) {
  source = "audio:\n  future: yes\n";
  auto r = run(); ASSERT_TRUE(r); EXPECT_TRUE(r.missing); EXPECT_EQ(20, brightness);
  ASSERT_TRUE(run(true));
  EXPECT_NE(std::string::npos, saved.find("  volume: 3\n"));
  source = saved; ASSERT_TRUE(run()); EXPECT_FALSE(run().missing);
}
TEST_F(ConfigFixture, UnknownPreservation) {
  source = "future: 'hello'\nnested:\n  branch:\n    value: 7\nlist:\n  - one\n  - two\naudio:\n  volume: 7\n  future: [1, {two: 2}]\n";
  std::string unknown = source;
  ASSERT_TRUE(run()); volume = 9;
  ASSERT_TRUE(run(true));
  EXPECT_NE(std::string::npos, saved.find("future: 'hello'"));
  EXPECT_NE(std::string::npos, saved.find("    value: 7"));
  EXPECT_NE(std::string::npos, saved.find("  - one\n  - two"));
  EXPECT_NE(std::string::npos, saved.find("  future: [1, {two: 2}]"));
  EXPECT_NE(std::string::npos, saved.find("  volume: 9"));
}
TEST_F(ConfigFixture, UnavailableIsKnownAndPreserved) {
  source = "imu: 90\nbrightness: 30\naudio:\n  volume: 4\n";
  auto r = run(); ASSERT_TRUE(r); EXPECT_EQ(0u, r.unknown); EXPECT_EQ(5, imu);
  ASSERT_TRUE(run(true)); EXPECT_NE(std::string::npos, saved.find("imu: 90\n"));
  hasImu = true; ASSERT_TRUE(run()); EXPECT_EQ(90, imu);
}
TEST_F(ConfigFixture, InvalidTypesAndRanges) {
  for (auto text : {"-1", "101", "trueish", "'20'", "[2]", "999999999999999999999"}) {
    source = std::string("brightness: ") + text + "\n";
    auto r = run(); ASSERT_TRUE(r); EXPECT_EQ(1u, r.invalid); EXPECT_EQ(20, brightness);
  }
}
TEST_F(ConfigFixture, MalformedAndWriteFailure) {
  for (auto text : {"brightness: [2\n", "future:\n  child: 1\n broken: 3\n", "future: 'unclosed\n"}) {
    source = text; auto original = source;
    EXPECT_FALSE(run(true)); EXPECT_EQ(original, source);
  }
  source = "brightness: 30\n"; auto original = source; failAt = 3;
  EXPECT_FALSE(run(true)); EXPECT_EQ(original, source);
}
TEST_F(ConfigFixture, DeepUnknownAndBounds) {
  for (unsigned i = 0; i < 20; ++i) source += std::string(i * 2, ' ') + "future:\n";
  source += std::string(40, ' ') + "value: [1, 2, {a: b}]\n";
  ASSERT_TRUE(run(true)); EXPECT_EQ(source, saved.substr(0, source.size()));
  source = "future: " + std::string(MaxLine, 'x'); EXPECT_FALSE(run(true));
  source.clear();
  for (unsigned i = 0; i < MaxDepth + 2; ++i) source += std::string(i, ' ') + "x:\n";
  EXPECT_FALSE(run(true)); EXPECT_LE(sizeof(Workspace), 2048u);
}
TEST_F(ConfigFixture, UnknownBlockScalar) {
  source = "future: |\n  hello: [this is text\n  world\nbrightness: 4\n";
  ASSERT_TRUE(run(true)); EXPECT_NE(std::string::npos, saved.find("  hello: [this is text\n"));
}
}
namespace {
TEST_F(ConfigFixture, UnknownSequenceOfMaps) {
  source = "future:\n  - a: 1\n    b:\n      c: 3\n  - nested:\n      - x\n      - y\n    sibling: yes\n";
  ASSERT_TRUE(run(true)); EXPECT_EQ(source, saved.substr(0, source.size()));
}
TEST_F(ConfigFixture, QuotedKnownKey) {
  source = "\"brightness\": 30\n"; ASSERT_TRUE(run()); brightness = 40;
  ASSERT_TRUE(run(true)); source = saved; brightness = 0;
  ASSERT_TRUE(run()); EXPECT_EQ(40, brightness);
}
}
namespace {
TEST_F(ConfigFixture, MalformedFlowRejected) {
  for (auto value : {"{a 1}", "[1,,2]", "{a: 1 b: 2}", "[1] trailing", "{[a]: b}"}) {
    source = std::string("future: ") + value + "\n";
    EXPECT_FALSE(run(true)) << value;
  }
}
}
namespace {
TEST_F(ConfigFixture, EscapedKeysDoNotCreateDuplicateSettings) {
  source = "\"bright\\u006eess\": 30\n";
  ASSERT_TRUE(run()); EXPECT_EQ(30, brightness);
  brightness = 40; ASSERT_TRUE(run(true));
  EXPECT_EQ(std::string::npos, saved.find("\nbrightness:"));
  source = saved; ASSERT_TRUE(run()); EXPECT_EQ(40, brightness);
}
TEST_F(ConfigFixture, InvalidEscapesRejected) {
  for (auto text : {"future: \"\\q\"\n", "future: \"\\uQQQQ\"\n", "future: [\"\\x00oops\"]\n"}) {
    source = text;
    // NUL escapes are valid YAML for unknown values and need no decoding.
    if (source.find("x00") != std::string::npos) EXPECT_TRUE(run(true));
    else EXPECT_FALSE(run(true));
  }
}
}
namespace {
TEST_F(ConfigFixture, IndentlessListsArePreserved) {
  source = "future:\n- a: 1\n  b: [2]\n- a: 3\nbrightness: 8\n";
  ASSERT_TRUE(run()); EXPECT_EQ(8, brightness);
  ASSERT_TRUE(run(true)); EXPECT_EQ(source, saved.substr(0, source.size()));
}
}
namespace {
TEST_F(ConfigFixture, PlainUnknownTextIsNotAFlowCollection) {
  source = "future: text [with an unmatched bracket and an apostrophe '\n";
  ASSERT_TRUE(run(true)); EXPECT_EQ(source, saved.substr(0, source.size()));
}
}
namespace {
TEST_F(ConfigFixture, DuplicateKnownFieldsAndSchemaOverflowFailCleanly) {
  source = "brightness: 1\nbrightness: 2\n";
  EXPECT_FALSE(run(true));
  auto result = process({}, {}, {MaxFields + 1, this, field, set, known}, workspace, false);
  EXPECT_FALSE(result);
}
TEST_F(ConfigFixture, ReadFailureIsReported) {
  auto result = process({nullptr, [](void*) { return -2; }, nullptr}, {},
                        {3, this, field, set, known}, workspace, false);
  EXPECT_FALSE(result);
}
}

TEST(ConfigInteger, DecimalFullRange) {
  const struct { int64_t value; const char* expected; } cases[] = {
    {0, "0"}, {42, "42"}, {-42, "-42"},
    {INT32_MAX, "2147483647"}, {INT32_MIN, "-2147483648"},
    {UINT32_MAX, "4294967295"},
    {INT64_C(1234567890123456789), "1234567890123456789"},
    {-INT64_C(1234567890123456789), "-1234567890123456789"},
    {INT64_MAX, "9223372036854775807"}, {INT64_MIN, "-9223372036854775808"}
  };
  for (const auto& c : cases) {
    char output[21];
    ASSERT_TRUE(formatInteger(output, sizeof(output), c.value));
    EXPECT_STREQ(c.expected, output);
    int64_t parsed;
    ASSERT_TRUE(integer(output, INT64_MIN, INT64_MAX, parsed));
    EXPECT_EQ(c.value, parsed);
  }
}
TEST(ConfigInteger, BufferBounds) {
  char output[22];
  memset(output, 'X', sizeof(output));
  ASSERT_TRUE(formatInteger(output, 21, INT64_MIN));
  EXPECT_EQ('X', output[21]);
  EXPECT_FALSE(formatInteger(output, 20, INT64_MIN));
  EXPECT_EQ(0, output[0]);
  EXPECT_FALSE(formatInteger(nullptr, 0, 0));
  EXPECT_FALSE(formatInteger(output, 1, 0));
  EXPECT_TRUE(formatInteger(output, 2, 0));
  EXPECT_STREQ("0", output);
}
