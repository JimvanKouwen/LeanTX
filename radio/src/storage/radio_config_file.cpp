// LeanTX radio configuration. GPL-2.0-or-later.
#include "edgetx.h"
#include "storage.h"
#include "radio_config_file.h"
#include "radio_config_adapter.h"
#include "analogs.h"
#include "hal/adc_driver.h"

namespace {
// FatFs cannot rename over an existing file. This is a short-lived transaction
// recovery name, removed after commit, never an ordinary configuration backup.
const char transactionPath[] = RADIO_PATH PATH_SEPARATOR "radio.yml.swap";
struct FileWorkspace {
  radio_config::Workspace parser;
  FIL source, destination;
  FILINFO fileInfo;
  char readBuffer[128];
  char writeBuffer[512];
  UINT written;
  UINT position, length;
  bool eof;
};
static FileWorkspace workspace;
static bool busy;
static bool missingFile;
static_assert(sizeof(FileWorkspace) <= 4096, "radio storage fixed RAM budget");
struct Guard {
  Guard() { busy = true; }
  ~Guard() { busy = false; }
};
int readByte(void*) {
  auto& w = workspace;
  if (w.position == w.length) {
    if (w.eof) return -1;
    if (f_read(&w.source, w.readBuffer, sizeof(w.readBuffer), &w.length) != FR_OK) return -2;
    w.position = 0;
    if (!w.length) { w.eof = true; return -1; }
  }
  return (unsigned char)w.readBuffer[w.position++];
}
bool flushOutput() {
  auto& w = workspace;
  UINT count = 0;
  if (f_write(&w.destination, w.writeBuffer, w.written, &count) != FR_OK || count != w.written)
    return false;
  w.written = 0;
  return true;
}
bool writeBytes(void*, const char* text, size_t size) {
  auto& w = workspace;
  while (size) {
    size_t n = min(size, sizeof(w.writeBuffer) - w.written);
    memcpy(w.writeBuffer + w.written, text, n);
    w.written += n; text += n; size -= n;
    if (w.written == sizeof(w.writeBuffer) && !flushOutput()) return false;
  }
  return true;
}
void rewindInput() { workspace.position = workspace.length = 0; workspace.eof = false; }
const char* recover() {
  FRESULT active = f_stat(RADIO_SETTINGS_YAML_PATH, &workspace.fileInfo);
  if (active != FR_OK && active != FR_NO_FILE && active != FR_NO_PATH) return SDCARD_ERROR(active);
  if (active == FR_OK) return nullptr;
  FRESULT old = f_stat(transactionPath, &workspace.fileInfo);
  if (old == FR_NO_FILE || old == FR_NO_PATH) return nullptr;
  if (old != FR_OK) return SDCARD_ERROR(old);
  FRESULT r = f_rename(transactionPath, RADIO_SETTINGS_YAML_PATH);
  return r == FR_OK ? nullptr : SDCARD_ERROR(r);
}
void defaults() {
  generalDefault();
  resetRadioConfigMetadata();
#if defined(COLORLCD)
  for (unsigned i = 0; i < MAX_KEY_SHORTCUTS; ++i) g_eeGeneral.configSetToolName(false, i, "");
  for (unsigned i = 0; i < MAX_QM_FAVORITES; ++i) g_eeGeneral.configSetToolName(true, i, "");
#endif
#if XPOTS_MULTIPOS_COUNT > 0
  for (unsigned i = 0; i < adcGetMaxInputs(ADC_INPUT_FLEX); ++i) {
    if (getPotType(i) != FLEX_MULTIPOS) continue;
    auto& c = reinterpret_cast<StepsCalibData&>(g_eeGeneral.calib[adcGetInputOffset(ADC_INPUT_FLEX) + i]);
    if (c.count >= XPOTS_MULTIPOS_COUNT) c.count = 0;
  }
#endif
  // Custom analog names live outside RadioData; clear them on every reload too.
  for (unsigned type = ADC_INPUT_MAIN; type <= ADC_INPUT_FLEX; ++type)
    for (unsigned i = 0; i < adcGetMaxInputs(type); ++i) analogSetCustomLabel(type, i, "", 0);
}
}

bool radioSettingsMissing() { return missingFile; }

const char* loadRadioSettingsYaml(bool checks) {
  (void)checks; // A configuration document has syntax/types, not a layout CRC.
  if (busy) return "radio configuration busy";
  Guard guard;
  missingFile = false;
  defaults();
  if (const char* error = recover()) return error;
  FRESULT r = f_open(&workspace.source, RADIO_SETTINGS_YAML_PATH, FA_READ | FA_OPEN_EXISTING);
  if (r != FR_OK) { missingFile = r == FR_NO_FILE || r == FR_NO_PATH; return SDCARD_ERROR(r); }
  auto schema = radioSettingsSchema();
  rewindInput();
  // Validate before applying anything; a malformed tail cannot leave a partially
  // loaded radio. The source remains open throughout all three passes.
  auto result = radio_config::process({nullptr, readByte, nullptr}, {}, schema, workspace.parser, false);
  if (result && f_lseek(&workspace.source, 0) != FR_OK) result.error = "configuration seek failed";
  unsigned hardwareInvalid = 0;
  if (result) {
    // User-configurable analog types determine which calibration representation
    // applies. Resolve them first, independently of document key order.
    rewindInput();
    result = radio_config::process({nullptr, readByte, nullptr}, {}, radioSettingsSchema(RadioConfigLoadPhase::Hardware), workspace.parser, true);
    hardwareInvalid = result.invalid;
    if (result && f_lseek(&workspace.source, 0) != FR_OK) result.error = "configuration seek failed";
  }
  if (result) {
    rewindInput();
    result = radio_config::process({nullptr, readByte, nullptr}, {}, radioSettingsSchema(RadioConfigLoadPhase::Values), workspace.parser, true);
    result.invalid += hardwareInvalid;
  }
  r = f_close(&workspace.source);
  if (!result || r != FR_OK) {
    defaults();
    return result.error ? result.error : SDCARD_ERROR(r);
  }
  if (result.missing || result.invalid) storageDirty(EE_GENERAL);
  TRACE("radio config: %u invalid, %u unknown, missing=%u", result.invalid, result.unknown, result.missing);
  return nullptr;
}

const char* loadRadioSettings() {
  const char* error = loadRadioSettingsYaml(true);
  if (!error) g_eeGeneral.chkSum = evalChkSum();
  postRadioSettingsLoad();
  return error;
}

const char* writeGeneralSettings() {
  if (busy) return "radio configuration busy";
  Guard guard;
  if (const char* error = recover()) return error;
  FRESULT r = f_open(&workspace.source, RADIO_SETTINGS_YAML_PATH, FA_READ | FA_OPEN_EXISTING);
  bool exists = r == FR_OK;
  if (!exists && r != FR_NO_FILE && r != FR_NO_PATH) return SDCARD_ERROR(r);
  r = f_open(&workspace.destination, RADIO_SETTINGS_TMPFILE_YAML_PATH, FA_CREATE_ALWAYS | FA_WRITE);
  if (r != FR_OK) { if (exists) f_close(&workspace.source); return SDCARD_ERROR(r); }
  rewindInput();
  workspace.written = 0;
  auto result = radio_config::process({nullptr, exists ? readByte : nullptr, nullptr},
                                     {nullptr, nullptr, writeBytes}, radioSettingsSchema(), workspace.parser, false);
  if (exists && f_close(&workspace.source) != FR_OK) result.error = "configuration source close failed";
  if (result && !flushOutput()) result.error = "configuration write failed";
  if (result && f_sync(&workspace.destination) != FR_OK) result.error = "configuration flush failed";
  if (f_close(&workspace.destination) != FR_OK) result.error = "configuration close failed";
  if (!result) {
    f_unlink(RADIO_SETTINGS_TMPFILE_YAML_PATH);
    return result.error;
  }
  if (exists) {
    r = f_stat(transactionPath, &workspace.fileInfo);
    if (r == FR_OK) r = f_unlink(transactionPath);
    else if (r == FR_NO_FILE) r = FR_OK;
    if (r != FR_OK) return SDCARD_ERROR(r);
    r = f_rename(RADIO_SETTINGS_YAML_PATH, transactionPath);
    if (r != FR_OK) return SDCARD_ERROR(r);
  }
  r = f_rename(RADIO_SETTINGS_TMPFILE_YAML_PATH, RADIO_SETTINGS_YAML_PATH);
  if (r != FR_OK) {
    if (exists && f_rename(transactionPath, RADIO_SETTINGS_YAML_PATH) != FR_OK)
      return "configuration commit failed; original retained in radio.yml.swap";
    return SDCARD_ERROR(r);
  }
  if (exists) f_unlink(transactionPath);
  return nullptr;
}

bool storageReadRadioSettings(bool checks)
{
  if (!sdMounted()) sdInit();
  return loadRadioSettingsYaml(checks) == nullptr;
}
