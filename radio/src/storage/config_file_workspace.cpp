// GPL-2.0-or-later.
#include "config_file_workspace.h"
#include "edgetx.h"
#include "storage.h"
namespace config_file {
__DMA Workspace workspace;
bool busy;
const char* recover(const char* path)
{
  if (strlen(path) > 255) return "configuration path too long";
  auto& w = workspace;
  strcpy(w.temporary, path); strcat(w.temporary, ".tmp");
  strcpy(w.recovery, path); strcat(w.recovery, ".swap");
  FRESULT active = f_stat(path, &w.fileInfo);
  if (active == FR_OK) return nullptr;
  if (active != FR_NO_FILE && active != FR_NO_PATH) return SDCARD_ERROR(active);
  FRESULT old = f_stat(w.recovery, &w.fileInfo);
  if (old == FR_NO_FILE || old == FR_NO_PATH) return nullptr;
  if (old != FR_OK) return SDCARD_ERROR(old);
  FRESULT r = f_rename(w.recovery, path);
  return r == FR_OK ? nullptr : SDCARD_ERROR(r);
}
const char* read(const char* path)
{
  auto& w = workspace;
  if (const char* error = recover(path)) return error;
  FRESULT r = f_open(&w.file, path, FA_READ | FA_OPEN_EXISTING);
  if (r != FR_OK) return SDCARD_ERROR(r);
  auto size = f_size(&w.file);
  const char* error = nullptr;
  if (size > config_stream::DocumentCapacity) error = "configuration file too large";
  else {
    UINT count = 0;
    r = f_read(&w.file, w.parser.document, size, &count);
    if (r != FR_OK || count != size) error = "configuration read failed";
    w.length = count;
    w.parser.document[count] = 0;
  }
  r = f_close(&w.file);
  return error ? error : r == FR_OK ? nullptr : SDCARD_ERROR(r);
}
static bool append(void*, const char* text, size_t size)
{
  auto& w = workspace;
  if (size > config_stream::DocumentCapacity - w.length) {
    w.writer.error = "configuration file too large";
    return false;
  }
  memcpy(w.parser.document + w.length, text, size);
  w.length += size;
  return true;
}
const char* save(const char* path, const config_stream::Document& schema)
{
  auto& w = workspace;
  if (const char* error = recover(path)) return error;
  w.length = 0;
  auto& writer = w.writer;
  writer.output = {nullptr, nullptr, append};
  writer.error = nullptr; writer.depth = 0;
  writer.write("---\n", 4);
  if (!schema.save) return "configuration is read only";
  schema.save(schema.context, writer, w.parser.field);
  if (writer.error) return writer.error;
  // Stat is enough: a save never reads or parses the previous document.
  FRESULT r = f_stat(path, &w.fileInfo);
  bool exists = r == FR_OK;
  if (!exists && r != FR_NO_FILE && r != FR_NO_PATH) return SDCARD_ERROR(r);
  r = f_open(&w.file, w.temporary, FA_CREATE_ALWAYS | FA_WRITE);
  if (r != FR_OK) return SDCARD_ERROR(r);
  UINT written = 0;
  const char* error = nullptr;
  r = f_write(&w.file, w.parser.document, w.length, &written);
  if (r != FR_OK || written != w.length) error = "configuration write failed";
  if (!error && f_sync(&w.file) != FR_OK) error = "configuration flush failed";
  if (f_close(&w.file) != FR_OK) error = "configuration close failed";
  if (error) { f_unlink(w.temporary); return error; }
  if (exists) {
    r = f_stat(w.recovery, &w.fileInfo);
    if (r == FR_OK) r = f_unlink(w.recovery);
    else if (r == FR_NO_FILE || r == FR_NO_PATH) r = FR_OK;
    if (r != FR_OK) return SDCARD_ERROR(r);
    r = f_rename(path, w.recovery);
    if (r != FR_OK) return SDCARD_ERROR(r);
  }
  r = f_rename(w.temporary, path);
  if (r != FR_OK) {
    if (exists && f_rename(w.recovery, path) != FR_OK)
      return "configuration commit failed; original retained in .swap";
    return SDCARD_ERROR(r);
  }
  if (exists) f_unlink(w.recovery);
  return nullptr;
}
}
