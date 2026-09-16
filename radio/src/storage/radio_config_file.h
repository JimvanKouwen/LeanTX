// LeanTX radio configuration. GPL-2.0-or-later.
#pragma once
// Existing storage entry points; all radio configuration work lives here.
const char* loadRadioSettingsYaml(bool checks);
const char* loadRadioSettings();
const char* writeGeneralSettings();
bool storageReadRadioSettings(bool checks);
bool radioSettingsMissing();
