// Shared FatFs buffers for serialized streaming configuration transactions.
// GPL-2.0-or-later.
#pragma once
#include "config_stream.h"
#include "ff.h"

namespace config_file
{
struct Workspace {
  config_stream::Workspace parser;
  FIL source, destination;
  FILINFO fileInfo;
  char readBuffer[128];
  char writeBuffer[512];
  UINT written;
  UINT position, length;
  bool eof;
};
extern Workspace workspace;
extern bool busy;
struct Guard {
  Guard() { busy = true; }
  ~Guard() { busy = false; }
};
static_assert(sizeof(Workspace) <= 4096,
              "configuration file workspace fixed RAM budget");
}  // namespace config_file
