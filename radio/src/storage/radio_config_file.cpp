// LeanTX sparse canonical radio YAML. GPL-2.0-or-later.
#include "edgetx.h"
#include "storage.h"
#include "radio_config_file.h"
#include "radio_config_adapter.h"
#include "config_file_workspace.h"
namespace {
bool missingFile;
}
bool radioSettingsMissing() { return missingFile; }
const char* loadRadioSettingsYaml(bool checks)
{
  (void)checks;
  if (config_file::busy) return "radio configuration busy";
  config_file::Guard guard;
  auto& w = config_file::workspace;
  missingFile = false;
  auto schema = beginRadioSettingsLoad();
  if (const char* error = config_file::read(RADIO_SETTINGS_YAML_PATH)) {
    FRESULT r = f_stat(RADIO_SETTINGS_YAML_PATH, &w.fileInfo);
    missingFile = r == FR_NO_FILE || r == FR_NO_PATH;
    return error;
  }
  auto result = config_stream::parse(w.parser.document, w.length, schema, w.parser);
  if (!result) return result.error;
  resolveRadioSettingsLoad(result);
  if (!result) return result.error;
  if (result.invalid) return "invalid radio configuration value";
  commitRadioSettingsLoad();
  return nullptr;
}
const char* loadRadioSettings()
{
  const char* error = loadRadioSettingsYaml(true);
  if (!error) { g_eeGeneral.chkSum = evalChkSum(); postRadioSettingsLoad(); }
  return error;
}
const char* writeGeneralSettings()
{
  if (config_file::busy) return "radio configuration busy";
  config_file::Guard guard;
  return config_file::save(RADIO_SETTINGS_YAML_PATH, radioSettingsDocument());
}
bool storageReadRadioSettings(bool checks)
{
  if (!sdMounted()) sdInit();
  return loadRadioSettingsYaml(checks) == nullptr;
}
