// LeanTX model configuration files. GPL-2.0-or-later.
#pragma once
struct ModelData;
const char* loadModelConfig(const char* path, ModelData& destination);
const char* saveModelConfig(const char* path);
// A rejected load must not let the previously active model overwrite this file.
bool modelConfigCanSave(const char* path);

namespace model_config
{
struct Header;
}
const char* loadModelConfigHeader(const char* path, model_config::Header&);
const char* saveModelConfigLabels(const char* path, const char* labels);
