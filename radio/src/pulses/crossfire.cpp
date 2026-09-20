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
#include <atomic>
#include "telemetry/crsf_device.h"
#include "telemetry/crossfire.h"

static bool deviceSlotAllowed[NUM_MODULES] = {};
static bool discoveryTurn[NUM_MODULES] = {};

#define CROSSFIRE_CH_BITS           11
#define CROSSFIRE_CENTER            0x3E0

#define MODULE_ALIVE_TIMEOUT  50                      // if the module has sent a valid frame within 500ms it is declared alive
static std::atomic<tmr10ms_t> lastAlive[NUM_MODULES];              // last time stamp module sent CRSF frames
static bool moduleAlive[NUM_MODULES];
static std::atomic<bool> resetPartial[NUM_MODULES];
static std::atomic<tmr10ms_t> receiverAlive[NUM_MODULES];
static std::atomic<bool> receiverSeen[NUM_MODULES];                 // module alive status

uint8_t createCrossfireBindFrame(uint8_t moduleIdx, uint8_t * frame)
{
  uint8_t * buf = frame;
  *buf++ = UART_SYNC;                                 /* device address */
  *buf++ = 7;                                         /* frame length */
  *buf++ = COMMAND_ID;                                /* cmd type */
  if (receiverSeen[moduleIdx] &&
      tmr10ms_t(get_tmr10ms() - receiverAlive[moduleIdx].load()) <= MODULE_ALIVE_TIMEOUT)
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
  consumeModelIdRequest(module);
  auto now = get_tmr10ms();
  if (tmr10ms_t(now - lastAlive[module].load(std::memory_order_relaxed)) > MODULE_ALIVE_TIMEOUT) {
    moduleAlive[module] = false;
  } else if (!moduleAlive[module]) {
    moduleAlive[module] = true;
    moduleState[module].counter = CRSF_FRAME_MODELID;
  }

  // Every management frame, including discovery, bind and model selection,
  // consumes the same credit. Only a channel frame replenishes that credit.
  if (deviceSlotAllowed[module]) {
    size_t size = 0;
    if (moduleState[module].counter == CRSF_FRAME_MODELID) {
      size = createCrossfireModelIDFrame(module, buffer);
      moduleState[module].counter = CRSF_FRAME_MODELID_SENT;
    } else if (moduleState[module].mode == MODULE_MODE_BIND) {
      size = createCrossfireBindFrame(module, buffer);
      moduleState[module].mode = MODULE_MODE_NORMAL;
    } else if (moduleState[module].mode == MODULE_MODE_NORMAL) {
      const bool discovering = !RfService::capabilities(module).queryCompleted;
      if (!discovering || !discoveryTurn[module])
        size = CrsfDevice::take(module, buffer, CROSSFIRE_FRAME_MAXLEN);
      if (size) discoveryTurn[module] = true;
      else if (discovering) {
        size = createCrossfirePingFrame(module, buffer);
        discoveryTurn[module] = false;
      }
    }
    if (size) {
      deviceSlotAllowed[module] = false;
      return size;
    }
  }
  deviceSlotAllowed[module] = true;
  return createCrossfireChannelsFrame(module, buffer, channels);
}

static void crossfireSetupMixerScheduler(uint8_t module)
{
  consumeModuleSync(module);
  ModuleSyncStatus& status = getModuleSyncStatus(module);
  if (status.isValid()) {
    mixerSchedulerSetPeriod(module, status.getAdjustedRefreshRate());
  } else {
    mixerSchedulerSetPeriod(module, CROSSFIRE_PERIOD(module));
  }
}

static bool _checkFrameCRC(uint8_t* rxBuffer)
{
  uint8_t len = rxBuffer[1];
  uint8_t crc = crc8(&rxBuffer[2], len - 1);
  return (crc == rxBuffer[len + 1]);
}

static void crossfireSendPulses(void* ctx, uint8_t* buffer, const int16_t* channels, uint8_t nChannels)
{
  if (!modulePortSerialTxCompleted(ctx)) return;
  auto mod_st = (etx_module_state_t*)ctx;
  auto module = modulePortGetModule(mod_st);
  auto drv = modulePortGetSerialDrv(mod_st->tx);
  auto drv_ctx = modulePortGetCtx(mod_st->tx);
  if (!drv || !drv->sendBuffer || !drv_ctx) return;
  crossfireSetupMixerScheduler(module);
  auto size = setupPulsesCrossfire(module, buffer, channels);
  if (size) drv->sendBuffer(drv_ctx, buffer, size);
}

static tmr10ms_t partialSince[NUM_MODULES];

static void crossfireProcessFrame(void* ctx, uint8_t* frame, uint8_t frame_len,
                                  uint8_t* buf, uint8_t* p_len)
{
  if (!ctx || !frame || !buf || !p_len) return;
  auto module = modulePortGetModule((etx_module_state_t*)ctx);
  if (module >= NUM_MODULES) return;
  auto& len = *p_len;
  const auto now = get_tmr10ms();
  if (resetPartial[module].exchange(false) || len >= CROSSFIRE_FRAME_MAXLEN ||
      (len && tmr10ms_t(now - partialSince[module]) > 10)) len = 0;
  for (unsigned i = 0; i < frame_len; ++i) {
    auto byte = frame[i];
    if (!len) {
      if (byte != RADIO_ADDRESS && byte != UART_SYNC) continue;
      partialSince[module] = now;
    }
    buf[len++] = byte;
    if (len == 2 && (buf[1] < 2 || buf[1] > CROSSFIRE_FRAME_MAXLEN - 2)) {
      len = 0;
      // The invalid length byte may itself be a new header.
      if (byte == RADIO_ADDRESS || byte == UART_SYNC) buf[len++] = byte;
    }
    if (len >= 4 && len == buf[1] + 2) {
      if (_checkFrameCRC(buf)) {
#if defined(BLUETOOTH)
        if (g_eeGeneral.bluetoothMode == BLUETOOTH_TELEMETRY &&
            bluetooth.state == BLUETOOTH_STATE_CONNECTED)
          bluetooth.write(buf, len);
#endif
        lastAlive[module] = now;
        if (buf[2] == LINK_ID && len >= 14) {
          receiverAlive[module] = now;
          receiverSeen[module] = buf[3 + RX_QUALITY_INDEX] != 0;
        }
        processCrossfireTelemetryFrame(module, buf, len);
      }
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
  if (module >= NUM_MODULES) return nullptr;
  discoveryTurn[module] = true;
  deviceSlotAllowed[module] = true; // select the model before the first RC frame
  moduleState[module].counter = CRSF_FRAME_MODELID;
  moduleAlive[module] = false;
  receiverSeen[module] = false;
  lastAlive[module] = get_tmr10ms() - MODULE_ALIVE_TIMEOUT - 1;
  getModuleSyncStatus(module) = ModuleSyncStatus{};
  resetCrossfireCapabilities(module);
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

      resetPartial[module] = true;

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

      resetPartial[module] = true;

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
  if (!mod_st) return;

  resetCrossfireCapabilities(modulePortGetModule(mod_st));

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
