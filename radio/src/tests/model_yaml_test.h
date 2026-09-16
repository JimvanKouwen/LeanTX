// Test helpers for the production semantic model adapter. GPL-2.0-or-later.
#pragma once
#include "storage/model_config_adapter.h"

inline void loadModelYamlStr(const char* text)
{
  config_stream::Workspace workspace;
  const char* cursor = text;
  auto result = config_stream::process({&cursor, [](void* ctx) {
    auto& p = *static_cast<const char**>(ctx);
    return *p ? int(static_cast<unsigned char>(*p++)) : -1;
  }, nullptr}, {}, model_config::beginLoad(), workspace, true);
  ASSERT_TRUE(result) << (result.error ? result.error : "");
  ASSERT_EQ(0u, result.invalid);
  ASSERT_EQ(nullptr, model_config::resolveLoad());
  model_config::commitLoad(g_model);
}

inline std::string saveModelYamlStr(ModelData& model)
{
  config_stream::Workspace workspace;
  std::string output;
  auto result = config_stream::process({}, {&output, [](void*) { return -1; },
    [](void* ctx, const char* text, size_t n) {
      static_cast<std::string*>(ctx)->append(text, n);
      return true;
    }}, model_config::schema(model), workspace, false);
  EXPECT_TRUE(result) << (result.error ? result.error : "");
  return output;
}
