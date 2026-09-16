// LeanTX radio configuration. GPL-2.0-or-later.
#pragma once
#include "config_stream.h"
config_stream::Schema radioSettingsSchema();
// Serialized by the storage guard; candidate and side effects remain private
// until a successful parse and close, followed by resolution and commit.
config_stream::Schema beginRadioSettingsLoad();
void resolveRadioSettingsLoad(config_stream::Result& result);
void commitRadioSettingsLoad();
