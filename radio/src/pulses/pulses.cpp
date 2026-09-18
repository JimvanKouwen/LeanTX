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

// #include "hal.h"
#include "edgetx.h"
#include "rf_internal.h"

#include "mixer_scheduler.h"
#include "hal/module_port.h"
#include "os/sleep.h"
#include "tasks/mixer_task.h"
#include "os/async.h"

#if defined(CROSSFIRE)
#include "pulses/crossfire.h"
#endif

static module_pulse_driver _module_drivers[MAX_MODULES];
static module_pulse_buffer _module_buffers[MAX_MODULES] __DMA_NO_CACHE;

void RfService::init()
{
  memset(_module_drivers, 0, sizeof(_module_drivers));
}

module_pulse_driver* pulsesGetModuleDriver(uint8_t module)
{
  return module < NUM_MODULES ? &(_module_drivers[module]) : nullptr;
}

ModuleState moduleState[NUM_MODULES];

void RfService::start()
{
  telemetryStart();
  mixerTaskStart();
}

void RfService::stop()
{
  telemetryStop();
  mixerTaskStop();

  for (uint8_t i = 0; i < MAX_MODULES; i++)
    pulsesStopModule(i);
}

void RfService::restart(uint8_t module)
{
  if (module >= NUM_MODULES) return;
  mixerTaskStop();

  // wait for the power output to be drained
  pulsesStopModule(module);
  sleep_ms(200);

  mixerTaskStart();
}

void pulsesRestartModuleUnsafe(uint8_t module)
{
  if (module >= MAX_MODULES)
    return;

  auto mod_drv = pulsesGetModuleDriver(module);
  if (!mod_drv->drv) return;

  auto drv = mod_drv->drv;
  drv->deinit(mod_drv->ctx);
  mod_drv->ctx = drv->init(module);
}

static volatile bool _module_restart_queued[NUM_MODULES] = {false};

static void _setup_async_module_restart(void* p1, uint32_t p2)
{
  uint8_t module = (uint8_t)(uintptr_t)p1;
  _module_restart_queued[module] = false;

  if (!mixerTaskTryLock()) {
    // In case the mixer cannot be locked, try again later
    // and make the same function pending again.
    async_call(_setup_async_module_restart, &_module_restart_queued[module], p1,
               p2);
    return;
  }

  moduleState[module].forced_off = 1;

  uint32_t timeout = p2;
  moduleState[module].counter = timeout;

  mixerTaskUnlock();
}

// return true if the request could be posted to the timer queue
bool RfService::restartAsync(uint8_t module, uint8_t cnt_delay)
{
  if (module >= NUM_MODULES) return false;
  void* param1 = (void*)(uintptr_t)module;
  return async_call(_setup_async_module_restart,
                    &_module_restart_queued[module], param1, cnt_delay);
}

void RfService::settingsChanged(uint8_t module)
{
  if (module < NUM_MODULES) moduleState[module].settings_updated = 1;
}





ModuleSettingsMode RfService::mode(int moduleIndex)
{
  return moduleIndex >= 0 && moduleIndex < NUM_MODULES ?
      (ModuleSettingsMode)moduleState[moduleIndex].mode : MODULE_MODE_NORMAL;
}

void RfService::setMode(int moduleIndex, ModuleSettingsMode mode)
{
  if (moduleIndex >= 0 && moduleIndex < NUM_MODULES &&
      (mode == MODULE_MODE_NORMAL || mode == MODULE_MODE_BIND))
    moduleState[moduleIndex].mode = mode;
}

uint8_t getModuleType(uint8_t module)
{
  uint8_t type = g_model.moduleData[module].type;

#if defined(HARDWARE_INTERNAL_MODULE)
  if (module == INTERNAL_MODULE && isInternalModuleAvailable(type)) {
    return type;
  }
#endif

#if defined(HARDWARE_EXTERNAL_MODULE)
  if (module == EXTERNAL_MODULE && isExternalModuleAvailable(type)) {
    return type;
  }
#endif

  return MODULE_TYPE_NONE;
}

// Unknown and obsolete stored module types must never enable an RF driver.
uint8_t getRequiredProtocol(uint8_t module)
{
  return getModuleType(module) == MODULE_TYPE_CROSSFIRE
             ? PROTOCOL_CHANNELS_CROSSFIRE
             : PROTOCOL_CHANNELS_NONE;
}

static module_init_cb_t _on_module_init = nullptr;
static module_deinit_cb_t _on_module_deinit = nullptr;

void pulsesSetModuleInitCb(module_init_cb_t cb)
{
  _on_module_init = cb;
}

void pulsesSetModuleDeInitCb(module_deinit_cb_t cb)
{
  _on_module_deinit = cb;
}

static void _init_module(uint8_t module, const etx_proto_driver_t* drv)
{
  auto mod = &(_module_drivers[module]);
  void* ctx = drv->init(module);

  // TODO: module init failed somehow, we should handle this better...
  if (!ctx) {
    TRACE("Module #%d init failed", module);
    return;
  }

  mod->drv = drv;
  mod->ctx = ctx;

  // board specific hook
  if (_on_module_init)
    _on_module_init(module, drv);

  // power ON
  modulePortSetPower(module, true);
  TRACE("Module #%d init succeeded", module);
}

static void _deinit_module(uint8_t module)
{
  auto mod = &(_module_drivers[module]);
  if (!mod->drv) return;

  // scheduling OFF
  mixerSchedulerSetPeriod(module, 0);

  // board specific hook
  auto drv = mod->drv;
  if (_on_module_deinit)
    _on_module_deinit(module, drv);

  // de-init
  auto ctx = mod->ctx;
  drv->deinit(ctx);

  // power OFF
  modulePortSetPower(module, false);

  // clear
  memset(mod, 0, sizeof(module_pulse_driver));
  TRACE("Module #%d de-init succeeded", module);
}

static void pulsesEnableModule(uint8_t module, uint8_t protocol)
{
  _deinit_module(module);

  switch (protocol) {

#if defined(CROSSFIRE)
    case PROTOCOL_CHANNELS_CROSSFIRE:
      _init_module(module, &CrossfireDriver);
      break;
#endif

    default:
      break;
  }
}

// TODO: declare a function in telemetry
extern volatile uint8_t _telemetryIsPolling;

void RfService::stopModule(uint8_t module)
{
  if (module >= MAX_MODULES) return;

  while(_telemetryIsPolling) {
    // In case the telemetry timer is currently polling the port,
    // we give the timer task a chance to run and finish the polling.
    sleep_ms(1);
  }
  _deinit_module(module);

  auto& proto = moduleState[module].protocol;
  proto = PROTOCOL_CHANNELS_NONE;
}

static bool _handle_async_restart(uint8_t module)
{
  auto& state = moduleState[module];
  if (state.forced_off) {
    if (state.counter > 0) {
      _deinit_module(module);
      state.protocol = PROTOCOL_CHANNELS_NONE;
      --state.counter;
      return true;
    } else {
      state.forced_off = 0;
    }
  }
  return false;
}

void pulsesSendNextFrame(uint8_t module)
{
  if (module >= MAX_MODULES) return;

  uint8_t protocol = getRequiredProtocol(module);

  auto& state = moduleState[module];
  if (state.protocol != protocol || state.forced_off) {

    if (_telemetryIsPolling) {
      // In case the telemetry timer is currently polling the port,
      // we just yield in the hope it will be different next time.
      return;
    }

    if (_handle_async_restart(module))
      return;

    pulsesEnableModule(module, protocol);
    moduleState[module].protocol = protocol;
    return;
  }

  auto mod = &(_module_drivers[module]);
  if (mod->drv) {
    auto drv = mod->drv;
    auto ctx = mod->ctx;

    if (state.settings_updated) {
      if (drv->onConfigChange) drv->onConfigChange(ctx);
      state.settings_updated = 0;
    }

    // if previous frame not completed, skip this one
    if (drv->txCompleted && !drv->txCompleted(ctx)) return;

    uint8_t channelStart = min<unsigned>(g_model.moduleData[module].channelsStart,
        MAX_OUTPUT_CHANNELS - CROSSFIRE_CHANNELS_COUNT);
    const int16_t* channels = &channelOutputs[channelStart];
    uint8_t nChannels = CROSSFIRE_CHANNELS_COUNT;

    auto buffer = _module_buffers[module]._buffer;
    drv->sendPulses(ctx, buffer, channels, nChannels);
  }
}

void RfService::sendChannels()
{
  for (uint8_t i = 0; i < MAX_MODULES; i++) {
    pulsesSendNextFrame(i);
  }
}


void RfService::pollFrame(uint8_t module, const etx_proto_driver_t* drv)
{
  auto mod = pulsesGetModuleDriver(module);
  if (!mod || !mod->drv || !mod->ctx || (drv != mod->drv))
    return;

  auto ctx = mod->ctx;
  auto mod_st = (etx_module_state_t*)ctx;
  auto serial_drv = modulePortGetSerialDrv(mod_st->rx);
  auto serial_ctx = modulePortGetCtx(mod_st->rx);

  if (!serial_drv || !serial_ctx || !serial_drv->copyRxBuffer)
    return;

  uint8_t frame[TELEMETRY_RX_PACKET_SIZE];

  int frame_len = serial_drv->copyRxBuffer(serial_ctx, frame, TELEMETRY_RX_PACKET_SIZE);
  if (frame_len > 0) {

    LOG_TELEMETRY_WRITE_START();
    for (int i = 0; i < frame_len; i++) {
      telemetryMirrorSend(frame[i]);
      LOG_TELEMETRY_WRITE_BYTE(frame[i]);
    }

    uint8_t* rxBuffer = getTelemetryRxBuffer(module);
    uint8_t& rxBufferCount = getTelemetryRxBufferCount(module);
    drv->processFrame(ctx, frame, frame_len, rxBuffer, &rxBufferCount);
  }

}

static inline void pollTelemetry(uint8_t module, const etx_proto_driver_t* drv, void* ctx)
{
  if (!drv || !drv->processData) return;

  auto mod_st = (etx_module_state_t*)ctx;
  auto serial_drv = modulePortGetSerialDrv(mod_st->rx);
  auto serial_ctx = modulePortGetCtx(mod_st->rx);

  if (!serial_drv  || !serial_ctx || !serial_drv->getByte)
    return;

  uint8_t* rxBuffer = getTelemetryRxBuffer(module);
  uint8_t& rxBufferCount = getTelemetryRxBufferCount(module);

  uint8_t data;
  if (serial_drv->getByte(serial_ctx, &data) > 0) {
    LOG_TELEMETRY_WRITE_START();
    do {
      telemetryMirrorSend(data);
      drv->processData(ctx, data, rxBuffer, &rxBufferCount);
      LOG_TELEMETRY_WRITE_BYTE(data);
    } while (serial_drv->getByte(serial_ctx, &data) > 0);
  }
}

void RfService::pollTelemetry(uint8_t module)
{
  auto mod = pulsesGetModuleDriver(module);
  if (mod) ::pollTelemetry(module, mod->drv, mod->ctx);
}

#if defined(HARDWARE_INTERNAL_MODULE)
static ModuleSyncStatus moduleSyncStatus[NUM_MODULES];

ModuleSyncStatus &getModuleSyncStatus(uint8_t moduleIdx)
{
  return moduleSyncStatus[moduleIdx];
}
#else
static ModuleSyncStatus moduleSyncStatus;

ModuleSyncStatus &getModuleSyncStatus(uint8_t moduleIdx)
{
  return moduleSyncStatus;
}
#endif

ModuleSyncStatus::ModuleSyncStatus()
{
  memset(this, 0, sizeof(ModuleSyncStatus));
}

void ModuleSyncStatus::update(uint16_t newRefreshRate, int16_t newInputLag)
{
  if (!newRefreshRate)
    return;

  if (newRefreshRate < MIN_REFRESH_RATE)
    newRefreshRate = newRefreshRate * (MIN_REFRESH_RATE / (newRefreshRate + 1));
  else if (newRefreshRate > MAX_REFRESH_RATE)
    newRefreshRate = MAX_REFRESH_RATE;

  refreshRate = newRefreshRate;
  inputLag    = newInputLag;
  currentLag  = newInputLag;
  lastUpdate  = get_tmr10ms();

#if 0
  TRACE("[SYNC] update rate = %dus; lag = %dus",refreshRate,currentLag);
#endif
}

void ModuleSyncStatus::invalidate() {
  //make invalid after use
  currentLag = 0;
}

uint16_t ModuleSyncStatus::getAdjustedRefreshRate()
{
  int16_t lag = currentLag;
  int32_t newRefreshRate = refreshRate;

  if (lag == 0) {
    return refreshRate;
  }

  newRefreshRate += lag;

  if (newRefreshRate < MIN_REFRESH_RATE) {
      newRefreshRate = MIN_REFRESH_RATE;
  }
  else if (newRefreshRate > MAX_REFRESH_RATE) {
    newRefreshRate = MAX_REFRESH_RATE;
  }

  currentLag -= newRefreshRate - refreshRate;
#if 0
  TRACE("[SYNC] mod rate = %dus; lag = %dus",newRefreshRate,currentLag);
#endif

  return (uint16_t)newRefreshRate;
}

void ModuleSyncStatus::getRefreshString(char * statusText)
{
  if (!isValid()) {
    return;
  }

  char * tmp = statusText;
#if defined(DEBUG)
  *tmp++ = 'L';
  tmp = strAppendSigned(tmp, inputLag, 5);
  tmp = strAppend(tmp, "R");
  tmp = strAppendUnsigned(tmp, refreshRate, 5);
#else
  tmp = strAppend(tmp, "Sync ");
  tmp = strAppendUnsigned(tmp, refreshRate);
#endif
  tmp = strAppend(tmp, "us");
}

CrossfireModuleStatus crossfireModuleStatus[NUM_MODULES] = {};

bool RfService::active(uint8_t module)
{
  return module < NUM_MODULES && moduleState[module].protocol == PROTOCOL_CHANNELS_CROSSFIRE;
}

void RfService::requestModelId(uint8_t module)
{
  if (module < NUM_MODULES) moduleState[module].counter = CRSF_FRAME_MODELID;
}

void RfService::beginDiscovery(uint8_t module)
{
  if (module < NUM_MODULES && moduleState[module].counter != CRSF_FRAME_MODELID_SENT)
    requestModelId(module);
}

CrossfireModuleStatus RfService::capabilities(uint8_t module)
{
  return module < NUM_MODULES ? crossfireModuleStatus[module] : CrossfireModuleStatus{};
}

bool RfService::elrsVersionAtLeast(uint8_t module, uint8_t major, uint8_t minor)
{
  auto status = capabilities(module);
  return status.isELRS && (status.major > major ||
      (status.major == major && status.minor >= minor));
}

void RfService::updateSync(uint8_t module, uint16_t interval, int16_t offset)
{
  if (module < NUM_MODULES) getModuleSyncStatus(module).update(interval, offset);
}

void RfService::receiveDeviceInfo(uint8_t module, const uint8_t* frame, size_t length)
{
  // Extended device info: destination, origin, terminated name, 12 bytes of
  // serial/hardware/software version, parameter count and parameter version.
  if (module >= NUM_MODULES || !frame || length < 21 ||
      frame[1] + 2u != length || frame[2] != DEVICE_INFO_ID ||
      frame[4] != MODULE_ADDRESS) return;
  size_t nameEnd = 5;
  while (nameEnd < length && frame[nameEnd]) ++nameEnd;
  size_t info = nameEnd + 1;
  if (info + 15 != length) return;
  CrossfireModuleStatus status{};
  size_t nameLength = min<size_t>(nameEnd - 5, sizeof(status.name) - 1);
  memcpy(status.name, frame + 5, nameLength);
  status.isELRS = memcmp(frame + info, "ELRS", 4) == 0;
  status.major = frame[info + 9];
  status.minor = frame[info + 10];
  status.revision = frame[info + 11];
  status.queryCompleted = true;
  crossfireModuleStatus[module] = status;
  auto& config = g_model.moduleData[module].crsf;
  if (!elrsVersionAtLeast(module, 4, 0) &&
      (config.crsfArmingMode != ARMING_MODE_CH5 || config.crsfArmingCondition != 0)) {
    config.crsfArmingMode = ARMING_MODE_CH5;
    config.crsfArmingCondition = 0;
    storageDirty(EE_MODEL);
  }
}

ModuleSettingsMode getModuleMode(int module) { return RfService::mode(module); }
void setModuleMode(int module, ModuleSettingsMode mode) { RfService::setMode(module, mode); }

bool RfService::usesTxHardware(uint8_t module, const void* hardware)
{
  if (module >= NUM_MODULES || !hardware) return false;
  auto state = modulePortGetState(module);
  return state && state->tx.port && state->tx.port->hw_def == hardware;
}

bool RfService::setModulePower(int module, bool enabled)
{
  if (module < 0 || module >= NUM_MODULES) return false;
  modulePortSetPower(module, enabled);
  return true;
}

bool RfService::setBootPin(int module, bool enabled)
{
  if (module < 0 || module >= NUM_MODULES) return false;
  auto description = modulePortGetModuleDescription(module);
  if (!description || !description->set_bootcmd) return false;
  description->set_bootcmd(enabled);
  return true;
}
