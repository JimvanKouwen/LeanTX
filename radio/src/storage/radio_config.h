// LeanTX radio configuration. GPL-2.0-or-later.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace radio_config {
constexpr unsigned MaxDepth = 24;
constexpr unsigned MaxLine = 1024;
constexpr unsigned MaxPath = 256;
constexpr unsigned MaxFields = 1024;
constexpr unsigned MaxValue = 128;

// No runtime layout information crosses this interface. A field is a semantic
// path, a capability decision and an explicitly formatted scalar.
struct Field {
  char path[MaxPath];
  char value[MaxValue];
  bool available;
  bool required; // False for readable legacy aliases; never insert missing aliases.
  bool metadataOnly; // Internal adapter request: omit value formatting.
};
struct Schema {
  unsigned count;
  void* context;
  bool (*field)(void*, unsigned, Field&);
  bool (*set)(void*, unsigned, const char*);
  bool (*known)(void*, const char*); // Includes unavailable hardware instances.
  int (*resolve)(void*, const char*) = nullptr; // Optional direct path lookup, including aliases.
  bool (*describe)(void*, unsigned, Field&) = nullptr; // Optional cheap metadata enumeration.
};
struct Stream {
  void* context;
  int (*read)(void*); // byte, -1 EOF, -2 error
  bool (*write)(void*, const char*, size_t);
};
struct Result {
  const char* error;
  unsigned invalid;
  unsigned unknown;
  bool missing;
  explicit operator bool() const { return !error; }
};
// One reusable workspace; callers serialize access. No heap or document tree.
struct Workspace {
  char line[MaxLine];
  char path[MaxPath];
  Field field;
  uint8_t seen[(MaxFields + 7) / 8];
  struct Level {
    uint16_t indent, pathLength;
    uint8_t kind; // 0 undecided, 1 map, 2 sequence
  } levels[MaxDepth];
};
static_assert(sizeof(Workspace) <= 2048, "configuration workspace budget");
Result process(Stream input, Stream output, const Schema&, Workspace&, bool apply);
// Strict scalar conversion helpers shared by adapters and tests.
bool integer(const char*, int64_t minimum, int64_t maximum, int64_t&);
bool string(const char*, char*, size_t capacity, size_t& length);
}
