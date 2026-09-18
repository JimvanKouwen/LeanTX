/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#include "definitions.h"
#include "dataconstants.h"
#include "pulses_common.h"
#include "hal/module_driver.h"

#include "rf_service.h"

inline bool isModuleBeeping(uint8_t module)
{ return getModuleMode(module) == MODULE_MODE_BIND; }

// Compatibility entry points for platform lifecycle callers. State belongs to RF.
inline void pulsesStopModule(uint8_t module) { RfService::stopModule(module); }

inline void restartModule(uint8_t module) { RfService::restart(module); }
inline bool restartModuleAsync(uint8_t module, uint8_t delay)
{ return RfService::restartAsync(module, delay); }

inline void pulsesModuleSettingsUpdate(uint8_t module)
{ RfService::settingsChanged(module); }


inline void pulsesInit() { RfService::init(); }
inline void pulsesStart() { RfService::start(); }
inline void pulsesStop() { RfService::stop(); }

inline bool isModuleInBeepMode()
{
  if (getModuleMode(0) == MODULE_MODE_BIND)
    return true;

#if NUM_MODULES > 1
  if (getModuleMode(1) == MODULE_MODE_BIND)
    return true;
#endif

  return false;
}
