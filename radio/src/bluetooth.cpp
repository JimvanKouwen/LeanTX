#include "io/bluetooth_firmware_container.h"
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

#include <stdio.h>
#include "edgetx.h"

#include "bluetooth_driver.h"
#include "os/sleep.h"

#if defined(LOG_BLUETOOTH)
extern FIL g_bluetoothFile;
#endif

#if defined(PCBX9E)
#define BLUETOOTH_COMMAND_NAME         "TTM:REN-"
#define BLUETOOTH_ANSWER_NAME          "TTM:REN"
#define BLUETOOTH_COMMAND_BAUD_115200  "TTM:BPS-115200"
#else
#define BLUETOOTH_COMMAND_NAME         "AT+NAME"
#define BLUETOOTH_ANSWER_NAME          "OK+"
#define BLUETOOTH_COMMAND_BAUD_115200  "AT+BAUD115200"
#endif

#if defined(_MSC_VER)
  #define SWAP32(val)      (_byteswap_ulong(val))
#elif defined(__GNUC__ )
  #define SWAP32(val)      (__builtin_bswap32(val))
#endif

Bluetooth bluetooth;

void Bluetooth::write(const uint8_t * data, uint8_t length)
{
  BLUETOOTH_TRACE_VERBOSE("BT>");
  for (int i = 0; i < length; i++) {
    BLUETOOTH_TRACE_VERBOSE(" %02X", data[i]);
  }
  BLUETOOTH_TRACE_VERBOSE(CRLF);
  bluetoothWrite(data, length);
}

static const char _bt_crlf[] = "\r\n";

void Bluetooth::writeString(const char * str)
{
  BLUETOOTH_TRACE("BT> %s" CRLF, str);
  bluetoothWrite(str, strlen(str));
  bluetoothWrite(_bt_crlf, sizeof(_bt_crlf) - 1);
}

char * Bluetooth::readline(bool error_reset)
{
  uint8_t byte;

  while (true) {
    if (!bluetoothRead(&byte)) {
#if defined(PCBX9E)
      // X9E BT module can get unresponsive
      BLUETOOTH_TRACE("NO RESPONSE FROM BT MODULE" CRLF);
#endif
      return nullptr;
    }

    BLUETOOTH_TRACE_VERBOSE("%02X ", byte);

#if 0
    if (error_reset && byte == 'R' && bufferIndex == 4 && memcmp(buffer, "ERRO", 4)) {
#if defined(PCBX9E)  // X9E enter BT reset loop if following code is implemented
      BLUETOOTH_TRACE("BT Error..." CRLF);
#else
      BLUETOOTH_TRACE("BT Reset..." CRLF);
      bufferIndex = 0;
      bluetoothDisable();
      state = BLUETOOTH_STATE_OFF;
      wakeupTime = get_tmr10ms() + 100; /* 1s */
#endif
      return nullptr;
    }
    else
#endif

    if (byte == '\n') {
      if (bufferIndex > 2 && buffer[bufferIndex-1] == '\r') {
        buffer[bufferIndex-1] = '\0';
        bufferIndex = 0;
        BLUETOOTH_TRACE("BT< %s" CRLF, buffer);
        if (error_reset && !strcmp((char *)buffer, "ERROR")) {
#if defined(PCBX9E) // X9E enter BT reset loop if following code is implemented
          BLUETOOTH_TRACE("BT Error..." CRLF);
#else
          BLUETOOTH_TRACE("BT Reset..." CRLF);
          bluetoothDisable();
          state = BLUETOOTH_STATE_OFF;
          wakeupTime = get_tmr10ms() + 100; /* 1s */
#endif
          return nullptr;
        }
        else {
          if (!memcmp(buffer, "Central:", 8))
            strcpy(localAddr, (char *) buffer + 8);
          else if (!memcmp(buffer, "Peripheral:", 11))
            strcpy(localAddr, (char *) buffer + 11);
          return (char *) buffer;
        }
      }
      else {
        bufferIndex = 0;
      }
    }
    else {
      buffer[bufferIndex++] = byte;
      bufferIndex &= (BLUETOOTH_LINE_LENGTH-1);
    }
  }
}

#if defined(PCBX9E)
void Bluetooth::wakeup(void)
{
#if !defined(SIMU)
  if (!g_eeGeneral.bluetoothMode) {
    if (state != BLUETOOTH_INIT) {
      bluetoothDisable();
      state = BLUETOOTH_INIT;
    }
  }
  else {
    static tmr10ms_t waitEnd = 0;
    if (state != BLUETOOTH_STATE_IDLE) {
      if (state == BLUETOOTH_INIT) {
        bluetoothInit(BLUETOOTH_DEFAULT_BAUDRATE, true);
        char command[32];
        char * cur = strAppend(command, BLUETOOTH_COMMAND_NAME);
        uint8_t len = ZLEN(g_eeGeneral.bluetoothName);
        if (len > 0) {
          for (int i = 0; i < len; i++) {
            *cur++ = char2lower(g_eeGeneral.bluetoothName[i]);
          }
          *cur = '\0';
        }
        else {
          cur = strAppend(cur, FLAVOUR);
        }
        writeString(command);
        state = BLUETOOTH_WAIT_TTM;
        waitEnd = get_tmr10ms() + 25; // 250ms
      }
      else if (state == BLUETOOTH_WAIT_TTM) {
        if (get_tmr10ms() > waitEnd) {
          char * line = readline();
          if (strncmp(line, "OK+", 3)) {
            state = BLUETOOTH_STATE_IDLE;
          }
          else {
            bluetoothInit(BLUETOOTH_FACTORY_BAUDRATE, true);
            writeString("TTM:BPS-115200");
            state = BLUETOOTH_WAIT_BAUDRATE_CHANGE;
            waitEnd = get_tmr10ms() + 250; // 2.5s
          }
        }
      }
      else if (state == BLUETOOTH_WAIT_BAUDRATE_CHANGE) {
        if (get_tmr10ms() > waitEnd) {
          state = BLUETOOTH_INIT;
        }
      }
    }

  }
#endif
}
#else // PCBX9E
void Bluetooth::wakeup()
{
  if (state != BLUETOOTH_STATE_OFF) {
    if (bluetoothIsWriting()) {
      return;
    }
  }

  tmr10ms_t now = get_tmr10ms();

  if (now < wakeupTime)
    return;

  wakeupTime = now + 5; /* 50ms default */

  if (state == BLUETOOTH_STATE_FLASH_FIRMWARE) {
    return;
  }

#if defined(BT_PWR_GPIO)
  if (g_eeGeneral.bluetoothMode == BLUETOOTH_OFF) {
    bluetoothDisable();
    state = BLUETOOTH_STATE_OFF;
    wakeupTime = now + 10; /* 100ms */
    return;
  }
#endif

  if (g_eeGeneral.bluetoothMode != BLUETOOTH_TELEMETRY) {

    if (state != BLUETOOTH_STATE_OFF) {
      bluetoothDisable();
      state = BLUETOOTH_STATE_OFF;
    }
    wakeupTime = now + 10; /* 100ms */
  }
  else if (state == BLUETOOTH_STATE_OFF) {
    bluetoothInit(BLUETOOTH_FACTORY_BAUDRATE, true);
    state = BLUETOOTH_STATE_FACTORY_BAUDRATE_INIT;
  }

  if (state == BLUETOOTH_STATE_FACTORY_BAUDRATE_INIT) {
    writeString("AT+BAUD4");
    state = BLUETOOTH_STATE_BAUDRATE_SENT;
    wakeupTime = now + 10; /* 100ms */
  }
  else if (state == BLUETOOTH_STATE_BAUDRATE_SENT) {
    bluetoothInit(BLUETOOTH_DEFAULT_BAUDRATE, true);
    state = BLUETOOTH_STATE_BAUDRATE_INIT;
    readline(false);
    wakeupTime = now + 10; /* 100ms */
  }
  else if (state == BLUETOOTH_STATE_CONNECTED) {
    readline(); // Handle connection errors while forwarding CRSF telemetry.
  }
  else {
    char * line = readline();
    if (state == BLUETOOTH_STATE_BAUDRATE_INIT) {
      char command[32];
      char * cur = strAppend(command, BLUETOOTH_COMMAND_NAME);
      uint8_t len = ZLEN(g_eeGeneral.bluetoothName);
      if (len > 0) {
        for (int i = 0; i < len; i++) {
          *cur++ = char2lower(g_eeGeneral.bluetoothName[i]);
        }
        *cur = '\0';
      }
      else {
        cur = strAppend(cur, FLAVOUR);
      }
      writeString(command);
      state = BLUETOOTH_STATE_NAME_SENT;
    } else if (state == BLUETOOTH_STATE_NAME_SENT && (line != nullptr) &&
               (!strncmp(line, "OK+", 3) || !strncmp(line, "Central:", 8) ||
                !strncmp(line, "Peripheral:", 11))) {
      writeString("AT+TXPW2");
      state = BLUETOOTH_STATE_POWER_SENT;
    } else if (state == BLUETOOTH_STATE_POWER_SENT &&
               (line != nullptr) &&
               (!strncmp(line, "Central:", 8) ||
                !strncmp(line, "Peripheral:", 11))) {
      writeString("AT+ROLE0");
      state = BLUETOOTH_STATE_ROLE_SENT;
    } else if (state == BLUETOOTH_STATE_ROLE_SENT &&
               (line != nullptr) &&
               (!strncmp(line, "Central:", 8) ||
                !strncmp(line, "Peripheral:", 11))) {
      state = BLUETOOTH_STATE_IDLE;
    } else if (state == BLUETOOTH_STATE_IDLE &&
               (line != nullptr) && !strncmp(line, "Connected:", 10)) {
      strcpy(distantAddr, &line[10]); // TODO quick & dirty
      state = BLUETOOTH_STATE_CONNECTED;

    }
  }
}
#endif

uint8_t Bluetooth::bootloaderChecksum(uint8_t command, const uint8_t * data, uint8_t size)
{
  uint8_t sum = command;
  for (uint8_t i = 0; i < size; i++) {
    sum += data[i];
  }
  return sum;
}

uint8_t Bluetooth::read(uint8_t * data, uint8_t size, uint32_t timeout)
{
  watchdogSuspend(timeout / 10);

  uint8_t len = 0;
  while (len < size) {
    uint32_t elapsed = 0;
    uint8_t byte;
    while (!bluetoothRead(&byte)) {
      if (elapsed++ >= timeout) {
        return len;
      }
      sleep_ms(1);
    }
    data[len++] = byte;
  }
  return len;
}

#define BLUETOOTH_ACK   0xCC
#define BLUETOOTH_NACK  0x33

const char * Bluetooth::bootloaderWaitCommandResponse(uint32_t timeout)
{
  uint8_t response[2];
  if (read(response, sizeof(response), timeout) != sizeof(response)) {
    return "Bluetooth timeout";
  }

  if (response[0] != 0x00) {
    return "Bluetooth error";
  }

  if (response[1] == BLUETOOTH_ACK || response[1] == BLUETOOTH_NACK) {
    return nullptr;
  }

  return "Bluetooth error";
}

const char * Bluetooth::bootloaderWaitResponseData(uint8_t *data, uint8_t size)
{
  uint8_t header[2];
  if (read(header, 2) != 2) {
    return "Bluetooth timeout";
  }

  uint8_t len = header[0] - 2;
  uint8_t checksum = header[1];

  if (len > size) {
    return "Bluetooth error";
  }

  if (read(data, len) != len) {
    return "Bluetooth timeout";
  }

  if (bootloaderChecksum(0, data, len) != checksum) {
    return "Bluetooth CRC error";
  }

  return nullptr;
}

const char * Bluetooth::bootloaderSetAutoBaud()
{
  uint8_t packet[2] = {
    0x55,
    0x55
  };
  write(packet, sizeof(packet));
  return bootloaderWaitCommandResponse();
}

void Bluetooth::bootloaderSendCommand(uint8_t command, const void *data, uint8_t size)
{
  uint8_t packet[3] = {
    uint8_t(3 + size),
    bootloaderChecksum(command, (uint8_t *) data, size),
    command
  };
  write(packet, sizeof(packet));
  if (size > 0) {
    write((uint8_t *)data, size);
  }
}

void Bluetooth::bootloaderSendCommandResponse(uint8_t response)
{
  uint8_t packet[2] = {
    0x00,
    response
  };
  write(packet, sizeof(packet));
}

enum {
  CMD_DOWNLOAD = 0x21,
  CMD_GET_STATUS = 0x23,
  CMD_SEND_DATA = 0x24,
  CMD_SECTOR_ERASE = 0x26,
  CMD_GET_CHIP_ID = 0x28,
};

enum {
  CMD_RET_SUCCESS = 0x40,
};

constexpr uint32_t CC26XX_BOOTLOADER_SIZE = 0x00001000;
constexpr uint32_t CC26XX_FIRMWARE_BASE = CC26XX_BOOTLOADER_SIZE;

constexpr uint32_t CC26XX_PAGE_ERASE_SIZE = 0x1000;
constexpr uint32_t CC26XX_MAX_BYTES_PER_TRANSFER = 252;

const char * Bluetooth::bootloaderSendData(const uint8_t * data, uint8_t size)
{
  bootloaderSendCommand(CMD_SEND_DATA, data, size);
  return bootloaderWaitCommandResponse();
}

const char * Bluetooth::bootloaderReadStatus(uint8_t &status)
{
  bootloaderSendCommand(CMD_GET_STATUS);
  const char * result = bootloaderWaitCommandResponse();
  if (result)
    return result;
  result = bootloaderWaitResponseData(&status, 1);
  bootloaderSendCommandResponse(result == nullptr ? BLUETOOTH_ACK : BLUETOOTH_NACK);
  return result;
}

const char * Bluetooth::bootloaderCheckStatus()
{
  uint8_t status;
  const char * result = bootloaderReadStatus(status);
  if (result)
    return result;
  if (status != CMD_RET_SUCCESS)
    return "Wrong status";
  return nullptr;
}

const char * Bluetooth::bootloaderEraseFlash(uint32_t start, uint32_t size)
{
  uint32_t address = start;
  uint32_t end = start + size;
  while (address < end) {
    uint32_t addressBigEndian = SWAP32(address);
    bootloaderSendCommand(CMD_SECTOR_ERASE, &addressBigEndian, sizeof(addressBigEndian));
    const char * result = bootloaderWaitCommandResponse();
    if (result)
      return result;
    result = bootloaderCheckStatus();
    if (result)
      return result;
    address += CC26XX_PAGE_ERASE_SIZE;
  }
  return nullptr;
}

const char * Bluetooth::bootloaderStartWriteFlash(uint32_t start, uint32_t size)
{
  uint32_t cmdArgs[2] = {
    SWAP32(start),
    SWAP32(size),
  };
  bootloaderSendCommand(CMD_DOWNLOAD, cmdArgs, sizeof(cmdArgs));
  const char * result = bootloaderWaitCommandResponse();
  if (result)
    return result;
  result = bootloaderCheckStatus();
  if (result)
    return result;
  return result;
}

const char * Bluetooth::bootloaderWriteFlash(const uint8_t * data, uint32_t size)
{
  while (size > 0) {
    uint32_t len = min<uint32_t>(size, CC26XX_MAX_BYTES_PER_TRANSFER);
    const char * result = bootloaderSendData(data, len);
    if (result)
      return result;
    result = bootloaderCheckStatus();
    if (result)
      return result;
    data += len;
    size -= len;
  }
  return nullptr;
}

const char * Bluetooth::doFlashFirmware(const char * filename, ProgressHandler progressHandler)
{
  const char * result;
  FIL file;
  uint8_t buffer[CC26XX_MAX_BYTES_PER_TRANSFER * 4];
  UINT count;

  // Dummy command
  bootloaderSendCommand(0);
  result = bootloaderWaitCommandResponse(0);
  if (result)
    result = bootloaderSetAutoBaud();
  if (result)
    return result;

  // Get chip ID
  bootloaderSendCommand(CMD_GET_CHIP_ID);
  result = bootloaderWaitCommandResponse();
  if (result)
    return result;
  uint8_t id[4];
  result = bootloaderWaitResponseData(id, 4);
  bootloaderSendCommandResponse(result == nullptr ? BLUETOOTH_ACK : BLUETOOTH_NACK);

  if (f_open(&file, filename, FA_READ) != FR_OK) {
    return "Error opening file";
  }

  FrSkyFirmwareInformation * information = (FrSkyFirmwareInformation *)buffer;
  if (f_read(&file, buffer, sizeof(FrSkyFirmwareInformation), &count) != FR_OK || count != sizeof(FrSkyFirmwareInformation)) {
    f_close(&file);
    return "Format error";
  }

  progressHandler(getBasename(filename), STR_FLASH_ERASE, 0, 0);

  result = bootloaderEraseFlash(CC26XX_FIRMWARE_BASE, information->size);
  if (result) {
    f_close(&file);
    return result;
  }

  uint32_t size = information->size;
  progressHandler(getBasename(filename), STR_FLASH_WRITE, 0, size);

  result = bootloaderStartWriteFlash(CC26XX_FIRMWARE_BASE, size);
  if (result)
    return result;

  uint32_t done = 0;
  while (1) {
    progressHandler(getBasename(filename), STR_FLASH_WRITE, done, size);
    if (f_read(&file, buffer, min<uint32_t>(sizeof(buffer), size - done), &count) != FR_OK) {
      f_close(&file);
      return "Error reading file";
    }
    result = bootloaderWriteFlash(buffer, count);
    if (result)
      return result;
    done += count;
    if (done >= size) {
      f_close(&file);
      return nullptr;
    }
  }
}

const char * Bluetooth::flashFirmware(const char * filename, ProgressHandler progressHandler)
{
  progressHandler(getBasename(filename), STR_MODULE_RESET, 0, 0);

  state = BLUETOOTH_STATE_FLASH_FIRMWARE;

  pulsesStop();

  bluetoothInit(BLUETOOTH_BOOTLOADER_BAUDRATE, true); // normal mode
  watchdogSuspend(500 /*5s*/);
  sleep_ms(1000);

  bluetoothInit(BLUETOOTH_BOOTLOADER_BAUDRATE, false); // bootloader mode
  watchdogSuspend(500 /*5s*/);
  sleep_ms(1000);

  const char * result = doFlashFirmware(filename, progressHandler);

  AUDIO_PLAY(AU_SPECIAL_SOUND_BEEP1 );
  BACKLIGHT_ENABLE();

  if (result) {
    POPUP_WARNING(STR_FIRMWARE_UPDATE_ERROR, result);
  }
  else {
    POPUP_INFORMATION(STR_FIRMWARE_UPDATE_SUCCESS);
  }

  progressHandler(getBasename(filename), STR_MODULE_RESET, 0, 0);

  /* wait 1s off */
  watchdogSuspend(500 /*5s*/);
  sleep_ms(1000);

  state = BLUETOOTH_STATE_OFF;
  pulsesStart();

  return result;
}
