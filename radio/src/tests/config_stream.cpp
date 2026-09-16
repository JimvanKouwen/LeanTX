// Bounded sparse YAML tests. GPL-2.0-or-later.
#include "gtest/gtest.h"
#include "storage/config_stream.h"
#include <string>
#include <cstring>
#include <climits>
using namespace config_stream;
namespace {
struct ConfigFixture : testing::Test {
  Workspace workspace;
  int brightness = 20, volume = 3;
  uint8_t seen[1]{};
  std::string source, saved;
  static void enter(void* ctx, const Node& parent, const char* key, const char* text, Node& child, Result& result) {
    auto& t = *static_cast<ConfigFixture*>(ctx);
    child.section = -1;
    if (parent.section == 0 && !strcmp(key, "audio")) { child.section = 1; if (*text) result.error = "expected mapping"; return; }
    int id = parent.section == 0 && !strcmp(key, "brightness") ? 0 : parent.section == 1 && !strcmp(key, "volume") ? 1 : -1;
    if (id < 0) return;
    if (!mark(t.seen, id, sizeof(t.seen), result)) return;
    int64_t value;
    if (!integer(text, 0, 100, value)) ++result.invalid;
    else (id ? t.volume : t.brightness) = value;
  }
  Result load() {
    auto copy = source;
    Document schema{this, enter, nullptr, seen, sizeof(seen)};
    return parse(copy.data(), copy.size(), schema, workspace);
  }
};
TEST_F(ConfigFixture, MissingDefaultsUnknownAndQuotedKeys) {
  source = "unknown:\n  brightness: 99\n'audio':\n  \"volume\": 8 # comment\n";
  ASSERT_TRUE(load()); EXPECT_EQ(20, brightness); EXPECT_EQ(8, volume);
}
TEST_F(ConfigFixture, InvalidSyntaxAndSubsetLimits) {
  for (auto text : {"bad: [", "a:\n  b: 1\n c: 2", "a: 'bad", "a: &anchor x", "a: |\n  text", "---\na: 1\n---", "a: {b: 1}"}) {
    source = text; EXPECT_FALSE(load()) << text;
  }
}
TEST_F(ConfigFixture, DuplicateAndRangeRejection) {
  source = "brightness: 25\nbrightness: 26\n"; EXPECT_FALSE(load());
  source = "brightness: 101\n"; auto r = load(); EXPECT_TRUE(r); EXPECT_EQ(1u, r.invalid);
}
TEST_F(ConfigFixture, SparseWriterDefersEmptySections) {
  Writer writer{{&saved, nullptr, [](void* p, const char* s, size_t n) { static_cast<std::string*>(p)->append(s, n); return true; }}};
  writer.begin("empty"); writer.begin(17u); writer.end(); writer.end();
  writer.begin("audio"); writer.value("volume", "8"); writer.end();
  EXPECT_EQ("audio:\n  volume: 8\n", saved);
}
TEST_F(ConfigFixture, WriterFailurePropagates) {
  Writer writer{{nullptr, nullptr, [](void*, const char*, size_t) { return false; }}};
  writer.value("volume", "8"); EXPECT_NE(nullptr, writer.error);
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
