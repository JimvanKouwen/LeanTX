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

#include "edgetx.h"
#include "bluetooth_firmware_container.h"

const char * readFrSkyFirmwareInformation(const char * filename, FrSkyFirmwareInformation & data)
{
  FIL file;
  UINT count;

  if (f_open(&file, filename, FA_READ) != FR_OK) {
    return STR_NEEDS_FILE;
  }

  if (f_read(&file, &data, sizeof(data), &count) != FR_OK || count != sizeof(data)) {
    f_close(&file);
    return STR_DEVICE_FILE_ERROR;
  }

  uint32_t size = f_size(&file);
  f_close(&file);

  if (data.headerVersion != 1 && data.fourcc != 0x4B535246) {
    return STR_DEVICE_FILE_ERROR;
  }

  if (size != sizeof(data) + data.size) {
    return STR_DEVICE_FILE_ERROR;
  }

  return nullptr;
}

