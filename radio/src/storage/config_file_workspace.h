// Shared, serialized whole-document YAML transactions. GPL-2.0-or-later.
#pragma once
#include "config_stream.h"
#include "ff.h"
namespace config_file {
struct Workspace {
  config_stream::Workspace parser;
  config_stream::Writer writer;
  FIL file;
  FILINFO fileInfo;
  size_t length;
  char temporary[272], recovery[272];
};
extern Workspace workspace;
extern bool busy;
struct Guard { Guard() { busy = true; } ~Guard() { busy = false; } };
const char* recover(const char* path);
const char* read(const char* path);
const char* save(const char* path, const config_stream::Document&);
static_assert(sizeof(Workspace) < config_stream::DocumentCapacity + 6144,
              "fixed configuration workspace budget");
}
