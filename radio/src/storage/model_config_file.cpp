// LeanTX model configuration. GPL-2.0-or-later.
#include "model_config_file.h"

#include "config_file_workspace.h"
#include "edgetx.h"
#include "model_config_adapter.h"
#include "storage.h"

namespace
{
// FatFs cannot rename over an existing file. This is a short-lived transaction
// recovery name, removed after commit, never an ordinary configuration backup.
static char transactionPath[272], temporaryPath[272];
static char rejectedPath[256];
using config_file::busy;
using config_file::Guard;
using config_file::workspace;
int readByte(void*)
{
  auto& w = workspace;
  if (w.position == w.length) {
    if (w.eof) return -1;
    if (f_read(&w.source, w.readBuffer, sizeof(w.readBuffer), &w.length) !=
        FR_OK)
      return -2;
    w.position = 0;
    if (!w.length) {
      w.eof = true;
      return -1;
    }
  }
  return (unsigned char)w.readBuffer[w.position++];
}
bool flushOutput()
{
  auto& w = workspace;
  UINT count = 0;
  if (f_write(&w.destination, w.writeBuffer, w.written, &count) != FR_OK ||
      count != w.written)
    return false;
  w.written = 0;
  return true;
}
bool writeBytes(void*, const char* text, size_t size)
{
  auto& w = workspace;
  while (size) {
    size_t n = min(size, sizeof(w.writeBuffer) - w.written);
    memcpy(w.writeBuffer + w.written, text, n);
    w.written += n;
    text += n;
    size -= n;
    if (w.written == sizeof(w.writeBuffer) && !flushOutput()) return false;
  }
  return true;
}
void rewindInput()
{
  workspace.position = workspace.length = 0;
  workspace.eof = false;
}
const char* recover(const char* path)
{
  if (strlen(path) > 255) return "model path too long";
  strcpy(transactionPath, path);
  strcat(transactionPath, ".swap");
  strcpy(temporaryPath, path);
  strcat(temporaryPath, ".tmp");
  FRESULT active = f_stat(path, &workspace.fileInfo);
  if (active != FR_OK && active != FR_NO_FILE && active != FR_NO_PATH)
    return SDCARD_ERROR(active);
  if (active == FR_OK) return nullptr;
  FRESULT old = f_stat(transactionPath, &workspace.fileInfo);
  if (old == FR_NO_FILE || old == FR_NO_PATH) return nullptr;
  if (old != FR_OK) return SDCARD_ERROR(old);
  FRESULT r = f_rename(transactionPath, path);
  return r == FR_OK ? nullptr : SDCARD_ERROR(r);
}

}  // namespace

const char* loadModelConfig(const char* path, ModelData& destination)
{
  if (busy) return "model configuration busy";
  Guard guard;
  // Set before any fallible operation; clear only after the candidate commits.
  if (strlen(path) >= sizeof(rejectedPath)) return "model path too long";
  strcpy(rejectedPath, path);
  if (const char* error = recover(path)) return error;
  FRESULT r = f_open(&workspace.source, path, FA_READ | FA_OPEN_EXISTING);
  if (r != FR_OK) return SDCARD_ERROR(r);
  auto schema = model_config::beginLoad();
  rewindInput();
  auto result = config_stream::process({nullptr, readByte, nullptr}, {}, schema,
                                       workspace.parser, true);
  r = f_close(&workspace.source);
  if (!result || r != FR_OK)
    return result.error ? result.error : SDCARD_ERROR(r);
  if (result.invalid) return "invalid model configuration value";
  if (const char* error = model_config::resolveLoad()) return error;
  model_config::commitLoad(destination);
  rejectedPath[0] = 0;
  if (result.missing) storageDirty(EE_MODEL);
  return nullptr;
}

static const char* saveConfig(const char* path,
                              const config_stream::Schema& schema,
                              bool requireExisting = false)
{
  if (busy) return "model configuration busy";
  Guard guard;
  if (const char* error = recover(path)) return error;
  FRESULT r = f_open(&workspace.source, path, FA_READ | FA_OPEN_EXISTING);
  bool exists = r == FR_OK;
  if (requireExisting && !exists) return SDCARD_ERROR(r);
  if (!exists && r != FR_NO_FILE && r != FR_NO_PATH) return SDCARD_ERROR(r);
  r = f_open(&workspace.destination, temporaryPath,
             FA_CREATE_ALWAYS | FA_WRITE);
  if (r != FR_OK) {
    if (exists) f_close(&workspace.source);
    return SDCARD_ERROR(r);
  }
  rewindInput();
  workspace.written = 0;
  auto result = config_stream::process(
      {nullptr, exists ? readByte : nullptr, nullptr},
      {nullptr, nullptr, writeBytes}, schema, workspace.parser, false);
  if (exists && f_close(&workspace.source) != FR_OK)
    result.error = "configuration source close failed";
  if (result && !flushOutput()) result.error = "configuration write failed";
  if (result && f_sync(&workspace.destination) != FR_OK)
    result.error = "configuration flush failed";
  if (f_close(&workspace.destination) != FR_OK)
    result.error = "configuration close failed";
  if (!result) {
    f_unlink(temporaryPath);
    return result.error;
  }
  if (exists) {
    r = f_stat(transactionPath, &workspace.fileInfo);
    if (r == FR_OK)
      r = f_unlink(transactionPath);
    else if (r == FR_NO_FILE)
      r = FR_OK;
    if (r != FR_OK) return SDCARD_ERROR(r);
    r = f_rename(path, transactionPath);
    if (r != FR_OK) return SDCARD_ERROR(r);
  }
  r = f_rename(temporaryPath, path);
  if (r != FR_OK) {
    if (exists && f_rename(transactionPath, path) != FR_OK)
      return "configuration commit failed; original retained in model .swap "
             "file";
    return SDCARD_ERROR(r);
  }
  if (exists) f_unlink(transactionPath);
  return nullptr;
}

bool modelConfigCanSave(const char* path)
{
  return !*rejectedPath || strcmp(path, rejectedPath);
}

const char* saveModelConfig(const char* path)
{
  if (!modelConfigCanSave(path)) return "Model load failed; save blocked";
  return saveConfig(path, model_config::schema(g_model));
}

namespace
{
// Metadata reads skim unselected root sections byte by byte. They neither
// instantiate ModelData nor run schema resolution on mixer/screen payloads.
struct HeaderInput {
  char line[config_stream::MaxLine];
  unsigned position, length;
  bool selected;
  unsigned sections;
};
static HeaderInput headerInput;
int readHeaderByte(void*)
{
  auto& h = headerInput;
  for (;;) {
    if (h.position < h.length) return (unsigned char)h.line[h.position++];
    h.position = h.length = 0;
    int c;
    bool overflow = false;
    while ((c = readByte(nullptr)) >= 0 && c != '\n') {
      if (c == '\r') continue;
      if (h.length + 2 < sizeof(h.line))
        h.line[h.length++] = c;
      else
        overflow = true;
    }
    if (c == -2) return -2;
    if (!h.length && c == -1) return -1;
    h.line[h.length] = 0;
    if (h.line[0] != ' ' && h.line[0] != '#' && h.line[0]) {
      if (h.sections == 3) return -1;
      h.selected = !strncmp(h.line, "header:", 7) ||
                   !strncmp(h.line, "moduleData:", 11) ||
                   !strncmp(h.line, "'header':", 9) ||
                   !strncmp(h.line, "\"header\":", 9) ||
                   !strncmp(h.line, "'moduleData':", 13) ||
                   !strncmp(h.line, "\"moduleData\":", 13);
      if (h.selected) h.sections |= strstr(h.line, "moduleData") ? 2 : 1;
    }
    if (!h.selected) {
      h.length = 0;
      continue;
    }
    if (overflow) return -2;
    h.line[h.length++] = '\n';
  }
}
}  // namespace
const char* loadModelConfigHeader(const char* path,
                                  model_config::Header& destination)
{
  if (busy) return "model configuration busy";
  Guard guard;
  if (const char* error = recover(path)) return error;
  FRESULT r = f_open(&workspace.source, path, FA_READ | FA_OPEN_EXISTING);
  if (r != FR_OK) return SDCARD_ERROR(r);
  model_config::Header candidate{};
  headerInput = {};
  rewindInput();
  auto result = config_stream::process({nullptr, readHeaderByte, nullptr}, {},
                                       model_config::headerSchema(candidate),
                                       workspace.parser, true);
  r = f_close(&workspace.source);
  if (!result || r != FR_OK)
    return result.error ? result.error : SDCARD_ERROR(r);
  if (result.invalid) return "invalid model header";
  destination = candidate;
  return nullptr;
}
const char* saveModelConfigLabels(const char* path, const char* labels)
{
#if defined(STORAGE_MODELSLIST)
  model_config::Header header{};
  if (strlen(labels) >= sizeof(header.header.labels))
    return "model labels too long";
  strcpy(header.header.labels, labels);
  return saveConfig(path, model_config::headerSchema(header, true), true);
#else
  return "model labels unavailable";
#endif
}
