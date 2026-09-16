// LeanTX radio configuration. GPL-2.0-or-later.
#pragma once
#include "radio_config.h"
enum class RadioConfigLoadPhase { All, Hardware, Values };
radio_config::Schema radioSettingsSchema(RadioConfigLoadPhase phase = RadioConfigLoadPhase::All);

void resetRadioConfigMetadata();
