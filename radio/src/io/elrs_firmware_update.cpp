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

#include "os/sleep.h"

#include "hal.h"
#include "hal/module_port.h"

#include <stdio.h>
#include "edgetx.h"
#include "elrs_firmware_update.h"
#include "stk500.h"
#include "debug.h"

#include "timers_driver.h"
#include "hal/watchdog_driver.h"
#include "os/time.h"

#include <memory>

#if !defined(COLORLCD)
  #include "lib_file.h"
#endif



class ElrsFirmwareUpdateDriver
{
  ModuleIndex module;

  etx_module_state_t* mod_st = nullptr;

  bool init();
  bool getByte(uint8_t& byte) const;
  void sendByte(uint8_t byte) const;
  void sendBuffer(uint8_t* buffer, uint16_t size) const;
  void clear() const;
  void deinit();

  bool getRxByte(uint8_t& byte) const;
  bool checkRxByte(uint8_t byte) const;
  const char* waitForInitialSync();
  const char* getDeviceSignature(uint8_t* signature) const;
  const char* loadAddress(uint32_t offset) const;
  const char* progPage(uint8_t* buffer, uint16_t size) const;
  void leaveProgMode();

 public:
  ElrsFirmwareUpdateDriver(ModuleIndex module) :
      module(module)
  {
  }

  const char* flashFirmware(FIL* file, const char* label,
                            ProgressHandler progressHandler);
};

static const etx_serial_init serialInitParams = {
  .baudrate = 57600,
  .encoding = ETX_Encoding_8N1,
  .direction = ETX_Dir_TX_RX,
  .polarity = ETX_Pol_Normal,
};

bool ElrsFirmwareUpdateDriver::init()
{
  // ELRS bootloader uses the external bay's half-duplex serial line.
  if (module != EXTERNAL_MODULE) return false;
  mod_st = modulePortInitSerial(module, ETX_MOD_PORT_SPORT, &serialInitParams, false);
  if (!mod_st) return false;
  modulePortSetPower(module, true);
  return true;
}

bool ElrsFirmwareUpdateDriver::getByte(uint8_t & byte) const
{
  auto drv = modulePortGetSerialDrv(mod_st->rx);
  auto ctx = modulePortGetCtx(mod_st->rx);
  return drv->getByte(ctx, &byte) > 0;
}

void ElrsFirmwareUpdateDriver::sendByte(uint8_t byte) const
{
  auto drv = modulePortGetSerialDrv(mod_st->tx);
  auto ctx = modulePortGetCtx(mod_st->tx);
  drv->sendByte(ctx, byte);
}

void ElrsFirmwareUpdateDriver::sendBuffer(uint8_t* buffer, uint16_t size) const
{
  auto drv = modulePortGetSerialDrv(mod_st->tx);
  auto ctx = modulePortGetCtx(mod_st->tx);

  drv->waitForTxCompleted(ctx);
  drv->sendBuffer(ctx, buffer, size);
  drv->waitForTxCompleted(ctx);
}

void ElrsFirmwareUpdateDriver::clear() const
{
  auto drv = modulePortGetSerialDrv(mod_st->rx);
  auto ctx = modulePortGetCtx(mod_st->rx);
  drv->clearRxBuffer(ctx);
}

void ElrsFirmwareUpdateDriver::deinit()
{
  clear();
  modulePortSetPower(module, false);
  modulePortDeInit(mod_st);
}

bool ElrsFirmwareUpdateDriver::getRxByte(uint8_t & byte) const
{
  uint32_t time = time_get_ms();
  
  while ((time_get_ms() - time) < 100) {              // 100ms
    if (getByte(byte)) {
#if defined(DEBUG_EXT_MODULE_FLASH)
      TRACE("[RX] 0x%X", byte);
#endif
      return true;
    }
  }

  byte = 0;
  return false;
}

bool ElrsFirmwareUpdateDriver::checkRxByte(uint8_t byte) const
{
  uint8_t rxchar;
  return getRxByte(rxchar) ? rxchar == byte : false;
}

const char * ElrsFirmwareUpdateDriver::waitForInitialSync()
{
  uint8_t byte;
  tmr10ms_t now = get_tmr10ms();;

#if defined(DEBUG_EXT_MODULE_FLASH)
  TRACE("[Wait for Sync]");
#endif

  clear();
  do {

    // Send sync request
    sendByte(STK_GET_SYNC);
    sendByte(CRC_EOP);

    getRxByte(byte);
    WDG_RESET();

  } while ((byte != STK_INSYNC) && ((get_tmr10ms() - now) < 500)); // 5secs

  if ((get_tmr10ms() - now) > 500) {
    return STR_DEVICE_NO_RESPONSE;
  }

  if (byte != STK_INSYNC) {
#if defined(DEBUG_EXT_MODULE_FLASH)
    TRACE("[byte != STK_INSYNC]");
#endif
    return STR_DEVICE_NO_RESPONSE;
  }

  if (!checkRxByte(STK_OK)) {
#if defined(DEBUG_EXT_MODULE_FLASH)
    TRACE("[!checkRxByte(STK_OK)]");
#endif
    return STR_DEVICE_NO_RESPONSE;
  }

  // avoids sending STK_READ_SIGN with STK_OK
  // in case the receiver is too slow changing
  // to RX mode (half-duplex).
  sleep_ms(1);

  return nullptr;
}

const char * ElrsFirmwareUpdateDriver::getDeviceSignature(uint8_t * signature) const
{
  clear();

  // Read signature
  sendByte(STK_READ_SIGN);
  sendByte(CRC_EOP);

  if (!checkRxByte(STK_INSYNC))
    return STR_DEVICE_NO_RESPONSE;

  for (uint8_t i = 0; i < 4; i++) {
    if (!getRxByte(signature[i])) {
      return STR_DEVICE_FILE_WRONG_SIG;
    }
  }

  return nullptr;
}

const char * ElrsFirmwareUpdateDriver::loadAddress(uint32_t offset) const
{
  sendByte(STK_LOAD_ADDRESS);
  sendByte(offset & 0xFF); // low  byte
  sendByte(offset >> 8);   // high byte
  sendByte(CRC_EOP);

  if (!checkRxByte(STK_INSYNC) || !checkRxByte(STK_OK)) {
    return STR_DEVICE_NO_RESPONSE;
  }

  // avoids sending next page back-to-back with STK_OK
  // in case the receiver is to slow changing to RX mode (half-duplex).
  sleep_ms(1);

  return nullptr;
}

const char * ElrsFirmwareUpdateDriver::progPage(uint8_t * buffer, uint16_t size) const
{
  sendByte(STK_PROG_PAGE);

  // page size
  sendByte(size >> 8);
  sendByte(size & 0xFF);

  // flash/eeprom flag
  sendByte(0);

  sendBuffer(buffer, size);

  sendByte(CRC_EOP);

  if (!checkRxByte(STK_INSYNC))
    return STR_DEVICE_NO_RESPONSE;

  uint8_t byte;
  uint8_t retries = 4;
  do {
    getRxByte(byte);
    WDG_RESET();
  } while (!byte && --retries);

  if (!retries || (byte != STK_OK))
    return STR_DEVICE_WRONG_REQUEST;

  return nullptr;
}

void ElrsFirmwareUpdateDriver::leaveProgMode()
{
  sendByte(STK_LEAVE_PROGMODE);
  sendByte(CRC_EOP);

  // eat last sync byte
  checkRxByte(STK_INSYNC);
  deinit();
}

const char* ElrsFirmwareUpdateDriver::flashFirmware(
    FIL* file, const char* label, ProgressHandler progressHandler)
{
#if defined(SIMU)
  for (uint16_t i = 0; i < 100; i++) {
    progressHandler(label, STR_WRITING, i, 100);
    sleep_ms(30);
  }
  return nullptr;
#endif

  const char * result = nullptr;
  if (!init()) return "Initialisation error";

  /* wait 500ms for power on */
  watchdogSuspend(500 /*5s*/);
  sleep_ms(500);

  result = waitForInitialSync();
  if (result) {
    leaveProgMode();
    return result;
  }

  unsigned char signature[4]; // 3 bytes signature + STK_OK
  result = getDeviceSignature(signature);
  if (result) {
    leaveProgMode();
    return result;
  }

  uint8_t buffer[256];
  uint16_t pageSize = 128;
  uint32_t writeOffset = 0;

  if (signature[0] != 0x1E) {
    leaveProgMode();
    return STR_DEVICE_FILE_WRONG_SIG;
  }

  if (signature[1] == 0x55 && signature[2] == 0xAA) {
    pageSize = 256;
    writeOffset = 0x1000; // start offset (word address)
  }

  while (!f_eof(file)) {
    progressHandler(label, STR_WRITING, file->fptr, file->obj.objsize);

    UINT count = 0;
    memclear(buffer, pageSize);
    if (f_read(file, buffer, pageSize, &count) != FR_OK) {
      result = STR_DEVICE_FILE_ERROR;
      break;
    }

    if (!count)
      break;

    clear();

    result = loadAddress(writeOffset);
    if (result) {
      break;
    }

    result = progPage(buffer, pageSize);
    if (result) {
      break;
    }

    writeOffset += pageSize / 2;
  }

  if (f_eof(file)) {
    progressHandler(label, STR_WRITING, file->fptr, file->obj.objsize);
  }

  leaveProgMode();
  return result;
}

bool ElrsDeviceFirmwareUpdate::flashFirmware(const char * filename, ProgressHandler progressHandler)
{
  FIL file;

  if (f_open(&file, filename, FA_READ) != FR_OK) {
    POPUP_WARNING(STR_DEVICE_FILE_ERROR);
    return false;
  }

  pulsesStop();



  progressHandler(getBasename(filename), STR_DEVICE_RESET, 0, 0);

  /* wait 2s off */
  watchdogSuspend(500 /*5s*/);
  sleep_ms(3000);

  ElrsFirmwareUpdateDriver driver(module);
  const char * result = driver.flashFirmware(&file, getBasename(filename), progressHandler);
  f_close(&file);

  AUDIO_PLAY(AU_SPECIAL_SOUND_BEEP1);
  BACKLIGHT_ENABLE();

  if (result) {
    POPUP_WARNING(STR_FIRMWARE_UPDATE_ERROR, result);
  }
  else {
    POPUP_INFORMATION(STR_FIRMWARE_UPDATE_SUCCESS);
  }

  watchdogSuspend(50 /*500ms*/);
  pulsesStart();

  return result == nullptr;
}
