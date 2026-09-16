// LeanTX sparse canonical model YAML. GPL-2.0-or-later.
#include "model_config_file.h"
#include "config_file_workspace.h"
#include "edgetx.h"
#include "model_config_adapter.h"
#include "storage.h"
namespace {
char rejectedPath[256];
}
const char* loadModelConfig(const char* path, ModelData& destination)
{
  if (config_file::busy) return "model configuration busy";
  config_file::Guard guard;
  if (strlen(path) >= sizeof(rejectedPath)) return "model path too long";
  strcpy(rejectedPath, path);
  auto schema = model_config::beginLoad();
  if (const char* error = config_file::read(path)) return error;
  auto& w = config_file::workspace;
  auto result = config_stream::parse(w.parser.document, w.length, schema, w.parser);
  if (!result) return result.error;
  if (result.invalid) return "invalid model configuration value";
  if (const char* error = model_config::resolveLoad()) return error;
  model_config::commitLoad(destination);
  rejectedPath[0] = 0;
  return nullptr;
}
bool modelConfigCanSave(const char* path) { return !*rejectedPath || strcmp(path, rejectedPath); }
const char* saveModelConfig(const char* path)
{
  if (!modelConfigCanSave(path)) return "Model load failed; save blocked";
  if (config_file::busy) return "model configuration busy";
  config_file::Guard guard;
  return config_file::save(path, model_config::document(g_model));
}
const char* loadModelConfigHeader(const char* path, model_config::Header& destination)
{
  if (config_file::busy) return "model configuration busy";
  config_file::Guard guard;
  if (const char* error = config_file::read(path)) return error;
  auto& w = config_file::workspace;
  model_config::Header candidate{};
  auto result = config_stream::parse(w.parser.document, w.length, model_config::headerDocument(candidate), w.parser);
  if (!result) return result.error;
  if (result.invalid) return "invalid model header";
  destination = candidate;
  return nullptr;
}
const char* saveModelConfigLabels(const char* path, const char* labels)
{
#if defined(STORAGE_MODELSLIST)
  if (config_file::busy) return "model configuration busy";
  config_file::Guard guard;
  auto schema = model_config::beginLoad();
  auto& model = model_config::loadedCandidate();
  if (strlen(labels) >= sizeof(model.header.labels)) return "model labels too long";
  if (const char* error = config_file::read(path)) return error;
  auto& w = config_file::workspace;
  auto result = config_stream::parse(w.parser.document, w.length, schema, w.parser);
  if (!result) return result.error;
  if (result.invalid) return "invalid model configuration value";
  if (const char* error = model_config::resolveLoad()) return error;
  strcpy(model.header.labels, labels);
  // Serialize the isolated candidate, including staged screens; no live commit.
  return config_file::save(path, model_config::document(model));
#else
  return "model labels unavailable";
#endif
}
