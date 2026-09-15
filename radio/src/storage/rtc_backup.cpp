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

#include "rtc_backup.h"

#include "edgetx.h"
#include "hal/switch_driver.h"
#include "stamp.h"

namespace {
EmergencySnapshot snapshot __DMA;

uint32_t crc32(const uint8_t *data, unsigned size, uint32_t crc = 0xffffffff)
{
  while (size--) {
    crc ^= *data++;
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return crc;
}

// Strict RTC decoder: consume every complete token and produce exactly one image.
bool decodeSnapshot(const uint8_t *src, unsigned length)
{
  auto *dst = reinterpret_cast<uint8_t *>(&snapshot);
  unsigned written = 0;
  while (length) {
    uint8_t token = *src++;
    --length;
    if (!(token & 0x7f)) return false;
    unsigned zeros = (token & 0x80) ? ((token >> 4) & 7) :
                     (token & 0x40) ? (token & 0x3f) : 0;
    unsigned literals = (token & 0x80) ? (token & 15) :
                        (token & 0x40) ? 0 : token;
    if (literals > length || zeros + literals > sizeof(snapshot) - written)
      return false;
    memset(dst + written, 0, zeros);
    written += zeros;
    memcpy(dst + written, src, literals);
    written += literals;
    src += literals;
    length -= literals;
  }
  return written == sizeof(snapshot);
}

uint32_t buildTag()
{
  // GIT_STR ties the tag to the actual firmware image, since an incremental
  // build can leave this file's __DATE__/__TIME__ unchanged across commits.
  static const char build[] = __DATE__ " " __TIME__ " " FLAVOUR " " GIT_STR;
  return crc32(reinterpret_cast<const uint8_t *>(build), sizeof(build));
}

uint32_t integrity(const EmergencySnapshotHeader &header, const uint8_t *data)
{
  // Protect metadata as well as the encoded payload; magic is the commit marker.
  uint32_t crc = crc32(reinterpret_cast<const uint8_t *>(&header.version),
                      offsetof(EmergencySnapshotHeader, crc) -
                      offsetof(EmergencySnapshotHeader, version));
  return ~crc32(data, header.payloadLength, crc);
}
}

#if defined(SIMU)
RamBackup _ramBackup;
RamBackup *ramBackup = &_ramBackup;
#else
#if !defined(BKPSRAM_BASE) && defined(D3_BKPSRAM_BASE)
#define BKPSRAM_BASE D3_BKPSRAM_BASE
#endif
RamBackup *ramBackup = reinterpret_cast<RamBackup *>(BKPSRAM_BASE);
#endif

void rambackupWrite()
{
  // Invalidate before touching payload. Publish magic only after a full write.
  volatile uint32_t &magic = ramBackup->header.magic;
  magic = 0;
#if defined(SIMU)
  __asm__ volatile("" ::: "memory");
#else
  __DSB();
#endif
  captureEmergencySnapshot(snapshot);
  unsigned length = compress(ramBackup->data, sizeof(ramBackup->data),
                             reinterpret_cast<const uint8_t *>(&snapshot),
                             sizeof(snapshot));
  if (!length) return;
  EmergencySnapshotHeader header{};
  header.version = EMERGENCY_SNAPSHOT_VERSION;
  header.payloadLength = length;
  header.snapshotLength = sizeof(snapshot);
  header.buildTag = buildTag();
  header.crc = integrity(header, ramBackup->data);
  ramBackup->header = header;
#if defined(SIMU)
  __asm__ volatile("" ::: "memory");
#else
  __DSB();
#endif
  magic = EMERGENCY_SNAPSHOT_MAGIC;
  TRACE("Emergency snapshot raw=%u compressed=%u free=%u", unsigned(sizeof(snapshot)),
        length, unsigned(sizeof(ramBackup->data)) - length);
}

bool rambackupRestore()
{
  const auto header = ramBackup->header;
  if (header.magic != EMERGENCY_SNAPSHOT_MAGIC ||
      header.version != EMERGENCY_SNAPSHOT_VERSION ||
      header.buildTag != buildTag() || header.snapshotLength != sizeof(snapshot) ||
      !header.payloadLength || header.payloadLength > sizeof(ramBackup->data))
    return false;
  if (header.crc != integrity(header, ramBackup->data)) return false;
  if (!decodeSnapshot(ramBackup->data, header.payloadLength)) return false;
  // Only supported RF configurations may be restored.
  if (snapshot.radio.internalModule != MODULE_TYPE_NONE &&
      snapshot.radio.internalModule != MODULE_TYPE_CROSSFIRE) return false;
  for (const auto &module : snapshot.model.modules) {
    if (module.type != MODULE_TYPE_NONE && module.type != MODULE_TYPE_CROSSFIRE)
      return false;
    if (module.channelsCount < -8 || module.channelsCount > 24 ||
        unsigned(module.channelsStart) + module.channelsCount + 8 > MAX_OUTPUT_CHANNELS)
      return false;
  }
  restoreEmergencySnapshot(snapshot);
  return true;
}

bool rambackupRestoreOrReset()
{
  if (rambackupRestore()) return true;
  memset(&g_eeGeneral, 0, sizeof(g_eeGeneral));
  memset(&g_model, 0, sizeof(g_model));
  for (unsigned i = 0; i < MAX_FLEX_SWITCHES; ++i)
    switchConfigFlex_raw(i, -1);
  return false;
}
