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

#if !defined(SIMU)
#include "stm32_exti_driver.h"
#include "stm32_hal_ll.h"
#endif

#include "edgetx.h"
#include "mixer_scheduler.h"
#include "hal/module_driver.h"
#include "hal/module_port.h"

#include "crossfire.h"
#include "rf_internal.h"
#include "telemetry/crsf_device.h"
#include "telemetry/crossfire.h"

static bool deviceSlotAllowed[NUM_MODULES] = {};

#define CROSSFIRE_CH_BITS           11
#define CROSSFIRE_CENTER            0x3E0

#define MIN_FRAME_LEN 3

#define MODULE_ALIVE_TIMEOUT  50                      // if the module has sent a valid frame within 500ms it is declared alive
static tmr10ms_t lastAlive[NUM_MODULES];              // last time stamp module sent CRSF frames
static tmr10ms_t lastRxChunk[NUM_MODULES];
static bool moduleAlive[NUM_MODULES];                 // module alive status

uint8_t createCrossfireBindFrame(uint8_t moduleIdx, uint8_t * frame)
{
  uint8_t * buf = frame;
  *buf++ = UART_SYNC;                                 /* device address */
  *buf++ = 7;                                         /* frame length */
  *buf++ = COMMAND_ID;                                /* cmd type */
  if (crossfireTelemetryStreaming(moduleIdx))
    *buf++ = RECEIVER_ADDRESS;                        /* Destination is receiver (unbind) */
  else
    *buf++ = MODULE_ADDRESS;                          /* Destination is module */
  *buf++ = RADIO_ADDRESS;                             /* Origin Address */
  *buf++ = SUBCOMMAND_CRSF;                           /* sub command */
  *buf++ = SUBCOMMAND_CRSF_BIND;                      /* initiate bind */
  *buf++ = crc8_BA(frame + 2, 5);
  *buf++ = crc8(frame + 2, 6);
  return buf - frame;
}

uint8_t createCrossfirePingFrame(uint8_t moduleIdx, uint8_t * frame)
{
  uint8_t * buf = frame;
  *buf++ = UART_SYNC;                                 /* device address */
  *buf++ = 4;                                         /* frame length */
  *buf++ = PING_DEVICES_ID;                           /* cmd type */
  *buf++ = BROADCAST_ADDRESS;                         /* Destination Address */
  *buf++ = RADIO_ADDRESS;                             /* Origin Address */
  *buf++ = crc8(frame + 2, 3);
  return buf - frame;
}

uint8_t createCrossfireModelIDFrame(uint8_t moduleIdx, uint8_t * frame)
{
  uint8_t * buf = frame;
  *buf++ = UART_SYNC;                                 /* device address */
  *buf++ = 8;                                         /* frame length */
  *buf++ = COMMAND_ID;                                /* cmd type */
  *buf++ = MODULE_ADDRESS;                            /* Destination Address */
  *buf++ = RADIO_ADDRESS;                             /* Origin Address */
  *buf++ = SUBCOMMAND_CRSF;                           /* sub command */
  *buf++ = COMMAND_MODEL_SELECT_ID;                   /* command of set model/receiver id */
  *buf++ = g_model.header.modelId[moduleIdx];         /* model ID */
  *buf++ = crc8_BA(frame + 2, 6);
  *buf++ = crc8(frame + 2, 7);
  return buf - frame;
}

// Nominal channel range is [-1024:+1024]; clamp only for protocol encoding.
uint8_t createCrossfireChannelsFrame(uint8_t moduleIdx, uint8_t * frame, const int16_t * pulses)
{
  //
  // sends channel data and also communicates status information in status byte:
  // - arming status in Switch mode (bit 0)
  // - arming mode Switch or CH5 (bit 1)
  // - bits 2-7 spare
  //
  uint8_t * buf = frame;
  *buf++ = MODULE_ADDRESS;
  *buf++ = 25;                  // 1(ID) + 22(channel data) + 1(extra status byte) + 1(CRC)
  uint8_t * crc_start = buf;
  *buf++ = CHANNELS_ID;

  //
  // assemble channel data
  //
  uint32_t bits = 0;
  uint8_t bitsavailable = 0;
  for (int i=0; i<CROSSFIRE_CHANNELS_COUNT; i++) {
    uint32_t val = limit(0, CROSSFIRE_CENTER + (pulses[i] * 4) / 5, 2 * CROSSFIRE_CENTER);
    bits |= val << bitsavailable;
    bitsavailable += CROSSFIRE_CH_BITS;
    while (bitsavailable >= 8) {
      *buf++ = bits;
      bits >>= 8;
      bitsavailable -= 8;
    }
  }

  //
  // assemble status byte
  //
  ModuleData *md = &g_model.moduleData[moduleIdx];

  if (md->crsf.crsfArmingMode == ARMING_MODE_SWITCH) {
    *buf = readPhysicalSwitchCondition(md->crsf.crsfArmingCondition);
  } else {
    *buf = 0x02;                                    // flag arming mode CH5
  }

  buf++;
  
  //
  // add crc
  //
  *buf++ = crc8(crc_start, 24);

  return buf - frame;
}

size_t setupPulsesCrossfire(uint8_t module, uint8_t* buffer, const int16_t* channels)
{
  if (module >= NUM_MODULES || !buffer || !channels) return 0;
  auto p_buf = buffer;
  // Every management frame consumes the permission earned by a channel frame.
  // This also bounds discovery/reset/bind traffic when telemetry is silent.
  if (!deviceSlotAllowed[module]) {
    deviceSlotAllowed[module] = true;
    return createCrossfireChannelsFrame(module, buffer, channels);
  }
  deviceSlotAllowed[module] = false;
  {
    //
    // An ELRS module stores the RF parameters in a model specific way using the
    // modelID as index. If the module resets after it was initally initialized the modelID 
    // needs to be resent as otherwise the module assumes modelID 0 which leads to the
    // module using the stored RF parameters for the model with modelID 0. This is not only
    // annoying but also potentially dangerous as a receiver will no longer re-connect.
    //
    // Reasons for a module resetting might be:
    // - power surge
    // - internal non-recoverable error
    // - after flashing in WiFi mode
    // - putting the module in WiFi mode and exiting WiFi mode (LUA script)
    // 
    // This logic takes care of sending the modelID again after a module comes back to 
    // live after a module reset
    // 
    if(moduleState[module].counter != CRSF_FRAME_MODELID ) {            // skip the reset check logic if first init
      if((tmr10ms_t)(get_tmr10ms() - lastAlive[module]) > MODULE_ALIVE_TIMEOUT) {  // check if module has recently sent CRSF frames
        moduleAlive[module] = false;                                    // no, declare it as dead  
      } else {
        if(moduleAlive[module] == false) {                              // if the module was dead and came back to live, e.g. reset
          moduleAlive[module] = true;                                   // declare the module as alive
          moduleState[module].counter = CRSF_FRAME_MODELID;             // and send it the modelID again 
        }
      }
    }

    if (moduleState[module].counter == CRSF_FRAME_MODELID) {
      TRACE("[XF] ModelID %d", g_model.header.modelId[module]);
      p_buf += createCrossfireModelIDFrame(module, p_buf);
      moduleState[module].counter = CRSF_FRAME_MODELID_SENT;
    } else if (moduleState[module].mode == MODULE_MODE_BIND) {
      p_buf += createCrossfireBindFrame(module, p_buf);
      moduleState[module].mode = MODULE_MODE_NORMAL;
    } else if (moduleState[module].counter == CRSF_FRAME_MODELID_SENT && !crossfireModuleStatus[module].queryCompleted) {
      p_buf += createCrossfirePingFrame(module, p_buf);
    } else if (moduleState[module].mode == MODULE_MODE_NORMAL &&
               (p_buf += CrsfDevice::take(module, p_buf, CROSSFIRE_FRAME_MAXLEN)) != buffer) {
      // The queue owns request storage until the complete copy above finishes.
    } else {
      p_buf += createCrossfireChannelsFrame(module, p_buf, channels);
      deviceSlotAllowed[module] = true;
    }
  }
  return p_buf - buffer;
}

static void crossfireSetupMixerScheduler(uint8_t module, size_t frameSize)
{
  ModuleSyncStatus& status = getModuleSyncStatus(module);
  uint32_t period = status.isValid() ? status.getAdjustedRefreshRate() : CROSSFIRE_PERIOD(module);
  uint32_t baud = 400000;
#if defined(HARDWARE_INTERNAL_MODULE)
  if (module == INTERNAL_MODULE) baud = INT_CROSSFIRE_BAUDRATE;
#endif
#if defined(HARDWARE_EXTERNAL_MODULE)
  if (module == EXTERNAL_MODULE) baud = EXT_CROSSFIRE_BAUDRATE;
#endif
  // Allow the actual frame to leave the UART before the next slot, including
  // maximum-size management requests at the lowest supported baud rate.
  uint32_t wireTime = (frameSize * 10000000u + baud - 1) / baud + 100;
  mixerSchedulerSetPeriod(module, limit<uint32_t>(MIN_REFRESH_RATE,
                                                 max(period, wireTime), MAX_REFRESH_RATE));
}

static bool _checkFrameCRC(uint8_t* rxBuffer)
{
  uint8_t len = rxBuffer[1];
  uint8_t crc = crc8(&rxBuffer[2], len - 1);
  // TRACE("[XF] crc = %d; pkt = %d", crc, rxBuffer[len + 1]);
  return (crc == rxBuffer[len + 1]);
}

static void crossfireSendPulses(void* ctx, uint8_t* buffer, const int16_t* channels, uint8_t nChannels)
{
  auto mod_st = (etx_module_state_t*)ctx;
  auto module = modulePortGetModule(mod_st);
  if (!buffer || !channels || nChannels < CROSSFIRE_CHANNELS_COUNT) return;
  auto size = setupPulsesCrossfire(module, buffer, channels);
  crossfireSetupMixerScheduler(module, size);

  auto drv = modulePortGetSerialDrv(mod_st->tx);
  auto drv_ctx = modulePortGetCtx(mod_st->tx);
  drv->sendBuffer(drv_ctx, buffer, size);
}

static bool _lenIsSane(uint32_t len)
{
  // packet len must be at least 3 bytes (type + payload + crc)
  // and 2 bytes < MAX (hdr + len)
  return len >= 4 && len <= CROSSFIRE_FRAME_MAXLEN && len <= TELEMETRY_RX_PACKET_SIZE;
}

static bool _validHdr(uint8_t* buf)
{
  return buf[0] == RADIO_ADDRESS || buf[0] == UART_SYNC;
}

static uint8_t* _processFrames(void* ctx, uint8_t* buf, uint8_t& len)
{
  uint8_t* p_buf = buf;
  while (len >= MIN_FRAME_LEN) {

    if (!_validHdr(p_buf)) {
      TRACE("[XF] skipping invalid start bytes");
      do { p_buf++; len--; } while(len > 0 && !_validHdr(p_buf));
      if (len < MIN_FRAME_LEN) break;
    }

    uint32_t pkt_len = p_buf[1] + 2;
    if (!_lenIsSane(pkt_len)) {
      TRACE("[XF] pkt len error (%d)", pkt_len);
      len = 0;
      break;
    }

    if (pkt_len > (uint32_t)len) {
      // incomplete packet
      break;
    }

    if (!_checkFrameCRC(p_buf)) {
      TRACE("[XF] CRC error ");
    } else {
#if defined(BLUETOOTH)
      // TODO: generic telemetry mirror to BT
      if (g_eeGeneral.bluetoothMode == BLUETOOTH_TELEMETRY &&
          bluetooth.state == BLUETOOTH_STATE_CONNECTED) {
        bluetooth.write(p_buf, pkt_len);
      }
#endif
      auto mod_st = (etx_module_state_t*)ctx;
      auto module = modulePortGetModule(mod_st);
      lastAlive[module] = get_tmr10ms();                              // valid frame received, note timestamp
      processCrossfireTelemetryFrame(module, p_buf, pkt_len);
    }

    p_buf += pkt_len;
    len -= pkt_len;
  }
  
  return p_buf;
}

static void crossfireProcessFrame(void* ctx, uint8_t* frame, uint8_t frame_len,
                                  uint8_t* buf, uint8_t* p_len)
{
  if (!ctx || !buf || !p_len || (!frame && frame_len)) return;
  uint8_t& len = *p_len;
  auto module = modulePortGetModule((etx_module_state_t*)ctx);
  if (module >= NUM_MODULES) { len = 0; return; }
  if (len > CROSSFIRE_FRAME_MAXLEN ||
      (tmr10ms_t)(get_tmr10ms() - lastRxChunk[module]) > 10) len = 0;
  lastRxChunk[module] = get_tmr10ms();
  // Consume all input, retaining at most one bounded partial frame.
  for (unsigned i = 0; i < frame_len; ++i) {
    if (len == 0 && frame[i] != RADIO_ADDRESS && frame[i] != UART_SYNC) continue;
    buf[len++] = frame[i];
    if (len >= 2 && (buf[1] < 2 || buf[1] > CROSSFIRE_FRAME_MAXLEN - 2)) {
      len = 0;
      continue;
    }
    if (len >= 4 && len == buf[1] + 2) {
      _processFrames(ctx, buf, len);
      len = 0;
    }
  }
}

static const etx_serial_init crsfSerialParams = {
  .baudrate = 0,
  .encoding = ETX_Encoding_8N1,
  .direction = ETX_Dir_TX_RX,
  .polarity = ETX_Pol_Normal,
};

#if !defined(SIMU)

#if defined(INTERNAL_MODULE_CRSF)
static void _crsf_intmodule_frame_received(void*)
{
  telemetryFrameTrigger_ISR(INTERNAL_MODULE, &CrossfireDriver);
}
#endif

#if defined(HARDWARE_EXTERNAL_MODULE)
static void _crsf_extmodule_frame_received()
{
  telemetryFrameTrigger_ISR(EXTERNAL_MODULE, &CrossfireDriver);
}

// proxy trigger to avoid calling
// FreeRTOS methods from ISR with prio 0
static void _soft_irq_trigger(void* param)
{
  // detect spurious IDLE IRQ with empty buffer
  auto mod_rx = (etx_module_driver_t*)param;
  auto drv = modulePortGetSerialDrv(*mod_rx);
  auto ctx = modulePortGetCtx(*mod_rx);

  if (!drv || !ctx || !drv->getBufferedBytes) return;
  if (drv->getBufferedBytes(ctx) == 0) return;

#if defined(TELEMETRY_USE_CUSTOM_EXTI)
  stm32_exti_custom_trigger_swi(TELEMETRY_RX_FRAME_EXTI_LINE);
#else
  stm32_exti_trigger_swi(TELEMETRY_RX_FRAME_EXTI_LINE);
#endif
}
#endif

#endif // !SIMU

static void* crossfireInit(uint8_t module)
{
  // Select the new model before the first RC frame after any lifecycle restart.
  moduleState[module].counter = CRSF_FRAME_MODELID;
  deviceSlotAllowed[module] = true;
  moduleAlive[module] = false;
  lastAlive[module] = get_tmr10ms() - MODULE_ALIVE_TIMEOUT - 1;
  getModuleSyncStatus(module).refreshRate = 0;
  crossfireModuleStatus[module] = {};
  telemetryData.telemetryValid &= ~(1u << module);
  CrsfDevice::cancel();
  etx_module_state_t* mod_st = nullptr;
  etx_serial_init params(crsfSerialParams);

#if defined(INTERNAL_MODULE_CRSF)
  if (module == INTERNAL_MODULE) {
    params.baudrate = INT_CROSSFIRE_BAUDRATE;
    mod_st = modulePortInitSerial(module, ETX_MOD_PORT_UART, &params, false);

    if (mod_st) {
      auto drv = modulePortGetSerialDrv(mod_st->rx);
      auto ctx = modulePortGetCtx(mod_st->rx);

      auto& rx_count = getTelemetryRxBufferCount(INTERNAL_MODULE);
      rx_count = 0;

#if !defined(SIMU)
      if (drv && ctx && drv->setIdleCb) {
        drv->setIdleCb(ctx, _crsf_intmodule_frame_received, nullptr);
      }
#endif
    }
  }
#endif

#if defined(HARDWARE_EXTERNAL_MODULE)
  if (module == EXTERNAL_MODULE) {
    params.baudrate = EXT_CROSSFIRE_BAUDRATE;
    mod_st = modulePortInitSerial(module, ETX_MOD_PORT_SPORT, &params, false);

    if (mod_st) {
      auto drv = modulePortGetSerialDrv(mod_st->rx);
      auto ctx = modulePortGetCtx(mod_st->rx);

      auto& rx_count = getTelemetryRxBufferCount(EXTERNAL_MODULE);
      rx_count = 0;

#if !defined(SIMU)
      if (drv && ctx && drv->setIdleCb) {
        drv->setIdleCb(ctx, _soft_irq_trigger, &mod_st->rx);
#if defined(TELEMETRY_USE_CUSTOM_EXTI)
        stm32_exti_custom_enable(TELEMETRY_RX_FRAME_EXTI_LINE, 3,
                          _crsf_extmodule_frame_received);
#else
        stm32_exti_enable(TELEMETRY_RX_FRAME_EXTI_LINE, 0,
                          _crsf_extmodule_frame_received);
#endif

      }
#endif
    }

  }
#endif

  if (mod_st) {
    mixerSchedulerSetPeriod(module, CROSSFIRE_PERIOD(module));
  }

  return (void*)mod_st;
}

static void crossfireDeInit(void* ctx)
{
  CrsfDevice::cancel();
  auto mod_st = (etx_module_state_t*)ctx;

  memset(&crossfireModuleStatus[modulePortGetModule(mod_st)], 0,
         sizeof(CrossfireModuleStatus));

#if !defined(SIMU) && defined(HARDWARE_EXTERNAL_MODULE)
  if (mod_st && (modulePortGetModule(mod_st) == EXTERNAL_MODULE)) {
    auto drv = modulePortGetSerialDrv(mod_st->rx);
    auto ctx = modulePortGetCtx(mod_st->rx);
    if (drv && ctx && drv->setIdleCb) {
#if defined(TELEMETRY_USE_CUSTOM_EXTI)
      stm32_exti_custom_disable(TELEMETRY_RX_FRAME_EXTI_LINE);
#else
     stm32_exti_disable(TELEMETRY_RX_FRAME_EXTI_LINE);
#endif
    }
  }
#endif

  modulePortDeInit(mod_st);
}

const etx_proto_driver_t CrossfireDriver = {
  .protocol = PROTOCOL_CHANNELS_CROSSFIRE,
  .init = crossfireInit,
  .deinit = crossfireDeInit,
  .sendPulses = crossfireSendPulses,
  .processData = nullptr,
  .processFrame = crossfireProcessFrame,
  .onConfigChange = nullptr,
  .txCompleted = modulePortSerialTxCompleted,
};
