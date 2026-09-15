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

#include "emergency_snapshot.h"
#include "rlc.h"

constexpr unsigned RTC_BACKUP_CAPACITY = 4096;
static_assert(sizeof(EmergencySnapshotHeader) == 20, "RTC header layout");
static_assert(sizeof(EmergencySnapshot) <= RTC_BACKUP_CAPACITY - sizeof(EmergencySnapshotHeader),
              "Review emergency snapshot capacity before adding fields");
// RLC needs one control byte per run of up to 63 literal bytes, so
// incompressible input can expand by ceil(size/63) bytes.
constexpr unsigned RLC_MAX_LITERAL_RUN = 63;
constexpr unsigned EMERGENCY_SNAPSHOT_WORST_CASE_COMPRESSED_SIZE =
    sizeof(EmergencySnapshot) +
    (sizeof(EmergencySnapshot) + RLC_MAX_LITERAL_RUN - 1) / RLC_MAX_LITERAL_RUN;
static_assert(
    EMERGENCY_SNAPSHOT_WORST_CASE_COMPRESSED_SIZE <=
        RTC_BACKUP_CAPACITY - sizeof(EmergencySnapshotHeader),
    "Worst-case incompressible snapshot must still fit the RTC payload area");
struct RamBackup {
  EmergencySnapshotHeader header;
  uint8_t data[RTC_BACKUP_CAPACITY - sizeof(EmergencySnapshotHeader)];
};
static_assert(sizeof(RamBackup) == RTC_BACKUP_CAPACITY, "RTC RAM capacity");
extern RamBackup *ramBackup;
void rambackupWrite();
bool rambackupRestore();
// Boot wrapper: failed recovery leaves zeroed control configuration and RF off.
bool rambackupRestoreOrReset();
