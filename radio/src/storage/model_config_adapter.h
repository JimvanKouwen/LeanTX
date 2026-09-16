// LeanTX semantic model storage. GPL-2.0-or-later.
#pragma once
#include "config_stream.h"
#include "datastructs.h"
struct ModelData;
struct ModelHeader;
struct ModuleData;
namespace model_config
{
config_stream::Schema schema(ModelData&);
config_stream::Schema beginLoad();
const char* resolveLoad();
void commitLoad(ModelData&);
// Independent of ModelData layout; used by model selection and label edits.
struct Header {
  ModelHeader header;
  ModuleData moduleData[2];
};
config_stream::Schema headerSchema(Header&, bool labelsOnly = false);
}  // namespace model_config
