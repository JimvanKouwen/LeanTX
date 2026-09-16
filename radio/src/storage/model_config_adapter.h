// LeanTX semantic model storage. GPL-2.0-or-later.
#pragma once
#include "config_stream.h"
#include "datastructs.h"
struct ModelData;
struct ModelHeader;
struct ModuleData;
namespace model_config
{
config_stream::Document document(ModelData&);
config_stream::Document beginLoad();
const char* resolveLoad();
ModelData& loadedCandidate();
void commitLoad(ModelData&);
// Independent of ModelData layout; used by model selection and label edits.
struct Header {
  ModelHeader header;
  ModuleData moduleData[2];
};
config_stream::Document headerDocument(Header&);
}  // namespace model_config
