// Checked-in output of the retired EdgeTX YAML writer. GPL-2.0-or-later.
#pragma once
#include <fstream>
#include <string>
#include "location.h"

inline std::string legacyFixture(const std::string& name)
{
  std::ifstream file(std::string(TESTS_PATH) + "/fixtures/legacy/" + name,
                     std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), {});
}
