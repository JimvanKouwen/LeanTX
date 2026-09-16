// Bounded LeanTX YAML subset; semantic sections, no object layout metadata.
// GPL-2.0-or-later.
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace config_stream {
constexpr unsigned MaxDepth = 24, MaxLine = 1024, MaxPath = 256;
constexpr unsigned MaxFields = 1024, MaxValue = 1024;
constexpr unsigned DocumentCapacity = 16 * 1024;
// Serialized alongside the document workspace; scalar codecs never nest while
// retaining decoded text. This also bounds the small Pocket menus-task stack.
extern char scalarBuffer[MaxValue];
struct Field {
  char value[MaxValue];
  bool available = false;
  bool metadataOnly = false;
  void reset() { value[0] = 0; available = metadataOnly = false; }
};
struct Result {
  const char* error;
  unsigned invalid, unknown;
  bool missing;
  explicit operator bool() const { return !error; }
};
struct Node {
  int section = -1;
  unsigned index[3]{};
  const char* item = nullptr;
};
struct Stream {
  void* context;
  int (*read)(void*);
  bool (*write)(void*, const char*, size_t);
};
struct Writer {
  Stream output;
  const char* error = nullptr;
  unsigned depth = 0;
  struct Level { char key[64]; bool emitted; } levels[MaxDepth];
  bool write(const char*, size_t);
  void begin(const char*);
  void begin(unsigned);
  void end();
  void value(const char*, const char*);
};
// Explicit section callbacks; no schema enumeration or path resolution.
struct Document {
  void* context;
  void (*enter)(void*, const Node&, const char*, const char*, Node&, Result&);
  void (*save)(void*, Writer&, Field&);
  uint8_t* seen;
  unsigned seenBytes;
};
struct Workspace {
  char document[DocumentCapacity + 1];
  Field field;
  struct Level {
    Node node;
    uint16_t indent, nextIndex;
    uint8_t kind;
    bool pending;
  } levels[MaxDepth];
};
Result parse(char*, size_t, const Document&, Workspace&);
#if defined(SIMU)
Result process(Stream, Stream, const Document&, Workspace&, bool apply);
#endif
bool mark(uint8_t*, unsigned, unsigned, Result&);
bool formatInteger(char*, size_t, int64_t);
bool integer(const char*, int64_t, int64_t, int64_t&);
bool string(const char*, char*, size_t, size_t&);
}
