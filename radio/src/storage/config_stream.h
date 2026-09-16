// LeanTX streaming configuration. GPL-2.0-or-later.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace config_stream {
constexpr unsigned MaxDepth = 24;
constexpr unsigned MaxLine = 1024;
constexpr unsigned MaxPath = 256;
constexpr unsigned MaxFields = 1024;
constexpr unsigned MaxValue = 1024;

// No runtime layout information crosses this interface. A field is a semantic
// path, a capability decision and an explicitly formatted scalar.
struct Field {
  char path[MaxPath];
  char value[MaxValue];
  bool available;
  bool required; // False for readable legacy aliases; never insert missing aliases.
  unsigned capacity; // Optional adapter scalar capacity.
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
  // Optional indexed block sequences. Paths use decimal indices (items/0/name).
  bool (*sequence)(void*, const char*) = nullptr;
  // Large schemas supply a bounded bitmap; small schemas use Workspace::seen.
  uint8_t* seen = nullptr;
  unsigned seenBytes = 0;
  // Next leaf beneath a semantic prefix, or count. Avoids full-schema scans
  // while merging large indexed schemas.
  unsigned (*next)(void*, const char*, unsigned) = nullptr;
  bool (*preserve)(void*, unsigned, const char*) = nullptr;
  bool (*cover)(void*, unsigned, const char*, uint8_t*) = nullptr;
  bool (*emitSequence)(void*, const char*) = nullptr;
  // Read-only dynamic mappings. Called after syntax validation, including
  // empty mapping values; adapters own semantic validation and commit policy.
  bool (*visit)(void*, const char* path, const char* scalar) = nullptr;
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
    uint16_t nextIndex;
    uint8_t kind; // 0 undecided, 1 map, 2 sequence
  } levels[MaxDepth];
};
static_assert(sizeof(Workspace) <= 3072, "configuration workspace budget");
Result process(Stream input, Stream output, const Schema&, Workspace&, bool apply);
// Strict scalar conversion helpers shared by adapters and tests.
// Decimal output including sign and terminator; false if capacity is too small.
bool formatInteger(char* output, size_t capacity, int64_t value);
bool integer(const char*, int64_t minimum, int64_t maximum, int64_t&);
bool string(const char*, char*, size_t capacity, size_t& length);
}
