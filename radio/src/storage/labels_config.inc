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

int readLabelsByte(void*)
{
  auto& w = config_file::workspace;
  if (w.position == w.length) {
    if (f_read(&w.source, w.readBuffer, sizeof(w.readBuffer), &w.length) != FR_OK) return -2;
    w.position = 0;
    if (!w.length) return -1;
  }
  return static_cast<unsigned char>(w.readBuffer[w.position++]);
}

bool loadLabelsConfig()
{
  if (config_file::busy) return false;
  config_file::Guard guard;
  auto& w = config_file::workspace;
  FRESULT opened = f_open(&w.source, LABELSLIST_YAML_PATH, FA_READ | FA_OPEN_EXISTING);
  if (opened != FR_OK) return opened == FR_NO_FILE || opened == FR_NO_PATH;
  LabelsReader reader;
  config_stream::Schema schema{};
  schema.context = &reader;
  schema.known = [](void*, const char*) { return false; };
  schema.visit = visitLabels;
  bool ok = true;
  for (int pass = 0; pass < 2 && ok; ++pass) {
    reader = {};
    reader.commit = pass != 0;
    w.position = w.length = 0;
    if (f_lseek(&w.source, 0) != FR_OK) { ok = false; break; }
    auto result = config_stream::process({nullptr, readLabelsByte, nullptr}, {}, schema, w.parser, true);
    ok = bool(result) && !result.invalid;
  }
  return f_close(&w.source) == FR_OK && ok;
}
} // namespace
