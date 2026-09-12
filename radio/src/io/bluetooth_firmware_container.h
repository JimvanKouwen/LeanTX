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

#include "dataconstants.h"
#include "definitions.h"

#include "hal/module_port.h"

// The FrSky Bluetooth firmware container retains its original on-disk format.
constexpr uint8_t FIRMWARE_FAMILY_BLUETOOTH_CHIP = 4;

PACK(struct FrSkyFirmwareInformation {
  uint32_t fourcc;
  uint8_t headerVersion;
  uint8_t firmwareVersionMajor;
  uint8_t firmwareVersionMinor;
  uint8_t firmwareVersionRevision;
  uint32_t size;
  uint8_t productFamily;
  uint8_t productId;
  uint16_t crc;
});

const char * readFrSkyFirmwareInformation(const char * filename, FrSkyFirmwareInformation & data);

