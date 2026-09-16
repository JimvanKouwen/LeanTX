// Dynamic labels/cache mappings on the bounded streaming reader.
// GPL-2.0-or-later. Included by modelslist.cpp.
namespace {
struct LabelsReader {
  bool commit = false;
  ModelCell* model = nullptr;
  bool valid = false;
};

bool visitLabels(void* opaque, const char* path, const char* scalar)
{
  auto& reader = *static_cast<LabelsReader*>(opaque);
  char key[config_stream::MaxPath];
  strcpy(key, path);
  // The engine escapes slashes in mapping keys to keep path boundaries safe.
  char* item = strchr(key, '/');
  if (item) *item++ = 0;
  char* attr = item ? strchr(item, '/') : nullptr;
  if (attr) *attr++ = 0;
  if (attr && strchr(attr, '/')) return true;
  if (item) for (char* p = item; *p; ++p) if (*p == '\x01') *p = '/';
  bool labels = !strcasecmp(key, "labels");
  bool models = !strcasecmp(key, "models");
  if (!labels && !models && strcasecmp(key, "sort")) return true;
  if (item && strlen(item) > (labels ? LABELS_LENGTH : LEN_MODEL_FILENAME)) return false;
  char value[LABELS_LENGTH + 1];
  size_t length;
  if (!config_stream::string(scalar, value, sizeof(value) - 1, length)) return false;
  value[length] = 0;
  if (!strcasecmp(key, "sort") && !item) {
    int64_t number;
    if (!config_stream::integer(value, 0, SORT_COUNT - 1, number)) return false;
    if (reader.commit) modelslabels.setSortOrder(ModelsSortBy(number));
    return true;
  }
  if (!item) return !*value;
  if (!attr) {
    if (*value) return false;
    if (!reader.commit) return true;
    if (labels) modelslabels.addLabel(item);
    if (models) {
      reader.model = nullptr;
      reader.valid = false;
      for (auto& entry : modelslist.fileHashInfo) {
        if (entry.name != item || entry.celladded) continue;
        reader.model = new ModelCell(item);
        strcpy(reader.model->modelFinfoHash, entry.hash);
        modelslist.push_back(reader.model);
        entry.celladded = true;
        if (entry.curmodel) modelslist.setCurrentModel(reader.model);
        break;
      }
    }
    return true;
  }
  if (labels) {
    // Legacy semantics: presence of selected selects even "false".
    if (reader.commit && !strcasecmp(attr, "selected")) modelslabels.addFilteredLabel(item);
    return true;
  }
  int64_t number = 0;
  bool lastopen = !strcasecmp(attr, "lastopen");
  int module = -1, member = -1;
  for (int i = 0; i < NUM_MODULES; ++i) {
    char name[24];
    const char* formats[] = {MODULE_ID_STR, MODULE_TYPE_STR, MODULE_RFPROTOCOL_STR};
    for (int j = 0; j < 3; ++j) {
      snprintf(name, sizeof(name), formats[j], i);
      if (!strcasecmp(attr, name)) { module = i; member = j; }
    }
  }
  if (lastopen) {
    // Legacy timestamps accept base-zero integers.
    char* end;
    number = strtoll(value, &end, 0);
    if (!*value || *end || number < 0 || number > INT32_MAX) return false;
  } else if (module >= 0 && !config_stream::integer(value, 0, 255, number)) return false;
#if LEN_BITMAP_NAME > 0
  if (!strcasecmp(attr, "bitmap") && length > LEN_BITMAP_NAME) return false;
#endif
  if (!reader.commit || !reader.model) return true;
  auto& model = *reader.model;
  if (!strcasecmp(attr, "hash")) {
    reader.valid = !strcmp(model.modelFinfoHash, value);
    model.valid_rfData = reader.valid;
    model._isDirty = !reader.valid;
  } else if (lastopen) model.lastOpened = number;
  else if (reader.valid) {
    if (!strcasecmp(attr, "name")) model.setModelName(value);
#if LEN_BITMAP_NAME > 0
    else if (!strcasecmp(attr, "bitmap")) strcpy(model.modelBitmap, value);
#endif
    else if (!strcasecmp(attr, "labels")) {
      for (const auto& label : ModelMap::fromCSV(value)) modelslabels.addLabelToModel(label, &model);
    } else if (module >= 0) {
      if (member == 0) model.modelId[module] = number;
      if (member == 1) model.moduleData[module].type = number;
      if (member == 2) model.moduleData[module].subType = number;
    }
  }
  return true;
}

void enterLabels(void* ctx, const config_stream::Node& parent, const char* key,
                 const char* scalar, config_stream::Node& child, config_stream::Result& result)
{
  child = parent; child.section = -1;
  char path[config_stream::MaxPath];
  if (parent.section == 0) {
    if (!strcasecmp(key, "labels")) child.section = 1;
    else if (!strcasecmp(key, "models")) child.section = 2;
    else if (strcasecmp(key, "sort")) return;
    snprintf(path, sizeof(path), "%s", key);
  } else if (parent.section == 1 || parent.section == 2) {
    child.item = key; child.section = parent.section + 2;
    snprintf(path, sizeof(path), "%s/%s", parent.section == 1 ? "labels" : "models", key);
  } else if (parent.section == 3 || parent.section == 4) {
    snprintf(path, sizeof(path), "%s/%s/%s", parent.section == 3 ? "labels" : "models", parent.item, key);
  } else return;
  if (!visitLabels(ctx, path, scalar)) ++result.invalid;
}
bool loadLabelsConfig()
{
  if (config_file::busy) return false;
  config_file::Guard guard;
  auto& w = config_file::workspace;
  LabelsReader reader;
  config_stream::Document schema{};
  schema.context = &reader;
  schema.enter = enterLabels;
  // Validate before publishing dynamic model-list objects. Radio/model files
  // use one candidate and one parse; this separate cache keeps its two passes.
  for (int pass = 0; pass < 2; ++pass) {
    if (config_file::read(LABELSLIST_YAML_PATH)) {
      FRESULT r = f_stat(LABELSLIST_YAML_PATH, &w.fileInfo);
      return r == FR_NO_FILE || r == FR_NO_PATH;
    }
    reader = {}; reader.commit = pass != 0;
    auto result = config_stream::parse(w.parser.document, w.length, schema, w.parser);
    if (!result || result.invalid) return false;
  }
  return true;
}
} // namespace
