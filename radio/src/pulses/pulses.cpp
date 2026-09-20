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
#include <atomic>

#include "mixer_scheduler.h"
#include "hal/module_port.h"
#include "os/sleep.h"
#include "tasks/mixer_task.h"

#if defined(CROSSFIRE)
#include "pulses/crossfire.h"
#endif

// RX holds this gate only while copying UART bytes. Steady-state TX never
// touches it; lifecycle changes defer if a snapshot is in progress.
static std::atomic_flag portGate[NUM_MODULES] = {};
static std::atomic_flag parserGate[NUM_MODULES] = {};
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

static std::atomic<uint16_t> restartRequest[NUM_MODULES];
static std::atomic<bool> modelIdRequest[NUM_MODULES];
bool RfService::restartAsync(uint8_t module, uint8_t delay)
{
  if (module >= NUM_MODULES) return false;
  uint16_t empty = 0;
  return restartRequest[module].compare_exchange_strong(empty, uint16_t(delay) + 1);
}

void RfService::settingsChanged(uint8_t module)
{
  if (module < NUM_MODULES) moduleState[module].settings_updated = 1;
}

ModuleSettingsMode RfService::mode(int moduleIndex)
{
  return moduleIndex >= 0 && moduleIndex < NUM_MODULES ?
      (ModuleSettingsMode)moduleState[moduleIndex].mode.load() : MODULE_MODE_NORMAL;
}

void RfService::setMode(int moduleIndex, ModuleSettingsMode mode)
{
  if (moduleIndex >= 0 && moduleIndex < NUM_MODULES &&
      (mode == MODULE_MODE_NORMAL || mode == MODULE_MODE_BIND))
    moduleState[moduleIndex].mode = mode;
}

uint8_t getModuleType(uint8_t module)
{
  if (module >= NUM_MODULES) return MODULE_TYPE_NONE;
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
    return;
  }

  mod->drv = drv;
  mod->ctx = ctx;

  // board specific hook
  if (_on_module_init)
    _on_module_init(module, drv);

  // power ON
  modulePortSetPower(module, true);
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

void RfService::stopModule(uint8_t module)
{
  if (module >= NUM_MODULES) return;
  // Explicit lifecycle operation: intentional interruption of RC.
  const bool resume = mixerTaskRunning();
  if (mixerTaskInitialized()) mixerTaskStop();
  while (parserGate[module].test_and_set(std::memory_order_acquire)) sleep_ms(1);
  while (portGate[module].test_and_set(std::memory_order_acquire)) sleep_ms(1);
  _deinit_module(module);
  moduleState[module].protocol = PROTOCOL_CHANNELS_NONE;
  portGate[module].clear(std::memory_order_release);
  parserGate[module].clear(std::memory_order_release);
  if (resume) mixerTaskStart();
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

  auto restart = restartRequest[module].exchange(0);
  if (restart) {
    moduleState[module].counter = restart - 1;
    moduleState[module].forced_off = 1;
  }
  uint8_t protocol = getRequiredProtocol(module);

  auto& state = moduleState[module];
  if (state.protocol != protocol || state.forced_off) {

    if (parserGate[module].test_and_set(std::memory_order_acquire)) return;
    if (portGate[module].test_and_set(std::memory_order_acquire)) {
      parserGate[module].clear(std::memory_order_release);
      return;
    }
    if (!_handle_async_restart(module)) {
      pulsesEnableModule(module, protocol);
      // Failed initialization must be retried on the next slot.
      state.protocol = _module_drivers[module].drv ? protocol : PROTOCOL_CHANNELS_NONE;
    }
    portGate[module].clear(std::memory_order_release);
    parserGate[module].clear(std::memory_order_release);
    return;
  }

  auto mod = &(_module_drivers[module]);
  if (mod->drv) {
    auto drv = mod->drv;
    auto ctx = mod->ctx;

    if (state.settings_updated.exchange(0)) {
      if (drv->onConfigChange) drv->onConfigChange(ctx);
    }

    // if previous frame not completed, skip this one
    if (!ctx || !drv->sendPulses || !drv->txCompleted || !drv->txCompleted(ctx)) return;

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


void RfService::pollFrame(uint8_t module, const etx_proto_driver_t* expected)
{
  if (module >= NUM_MODULES || parserGate[module].test_and_set(std::memory_order_acquire)) return;
  uint8_t frame[TELEMETRY_RX_PACKET_SIZE];
  int length = 0;
  void* context = nullptr;
  const etx_proto_driver_t* driver = nullptr;
  if (!portGate[module].test_and_set(std::memory_order_acquire)) {
    auto mod = pulsesGetModuleDriver(module);
    if (mod && mod->drv && mod->ctx && mod->drv == expected) {
      auto st = (etx_module_state_t*)mod->ctx;
      auto serial = modulePortGetSerialDrv(st->rx);
      if (serial && serial->copyRxBuffer) {
        length = serial->copyRxBuffer(modulePortGetCtx(st->rx), frame, sizeof(frame));
        driver = mod->drv;
        context = mod->ctx;
      }
    }
    portGate[module].clear(std::memory_order_release);
  }
  // CRSF's context is a stable module-array address used only to identify the
  // module by the parser. No UART/context contents are accessed after release.
  if (length > 0 && length <= int(sizeof(frame)) && driver->processFrame) {
    LOG_TELEMETRY_WRITE_START();
    for (int i = 0; i < length; ++i) {
      telemetryMirrorSend(frame[i]);
      LOG_TELEMETRY_WRITE_BYTE(frame[i]);
    }
    auto& count = getTelemetryRxBufferCount(module);
    driver->processFrame(context, frame, length, getTelemetryRxBuffer(module), &count);
  }
  parserGate[module].clear(std::memory_order_release);
}

void RfService::pollTelemetry(uint8_t module)
{
#if defined(CROSSFIRE)
  pollFrame(module, &CrossfireDriver);
#endif
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
    newRefreshRate = MIN_REFRESH_RATE;
  else if (newRefreshRate > MAX_REFRESH_RATE)
    newRefreshRate = MAX_REFRESH_RATE;

  refreshRate = newRefreshRate;
  inputLag    = newInputLag;
  currentLag  = newInputLag;
  lastUpdate  = get_tmr10ms();

#if 0
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
static std::atomic_flag capabilityGate[NUM_MODULES] = {};
static std::atomic<bool> capabilityInvalid[NUM_MODULES];
static std::atomic<uint32_t> syncRequest[NUM_MODULES];

void resetCrossfireCapabilities(uint8_t module)
{
  capabilityInvalid[module] = true;
  syncRequest[module] = 0;
}

void consumeModuleSync(uint8_t module)
{
  auto packed = syncRequest[module].exchange(0);
  if (packed) getModuleSyncStatus(module).update(packed & 0xffff, int16_t(packed >> 16));
}

bool RfService::active(uint8_t module)
{
  return module < NUM_MODULES && moduleState[module].protocol == PROTOCOL_CHANNELS_CROSSFIRE;
}

void RfService::requestModelId(uint8_t module)
{
  if (module < NUM_MODULES) modelIdRequest[module] = true;
}

void consumeModelIdRequest(uint8_t module)
{
  if (modelIdRequest[module].exchange(false)) moduleState[module].counter = CRSF_FRAME_MODELID;
}

void RfService::beginDiscovery(uint8_t module)
{
  if (module < NUM_MODULES && moduleState[module].counter != CRSF_FRAME_MODELID_SENT)
    requestModelId(module);
}

CrossfireModuleStatus RfService::capabilities(uint8_t module)
{
  CrossfireModuleStatus result{};
  if (module >= NUM_MODULES || capabilityGate[module].test_and_set(std::memory_order_acquire)) return result;
  if (!capabilityInvalid[module]) result = crossfireModuleStatus[module];
  capabilityGate[module].clear(std::memory_order_release);
  return result;
}

bool RfService::elrsVersionAtLeast(uint8_t module, uint8_t major, uint8_t minor)
{
  auto status = capabilities(module);
  return status.isELRS && (status.major > major ||
      (status.major == major && status.minor >= minor));
}

void RfService::updateSync(uint8_t module, uint16_t interval, int16_t offset)
{
  if (module < NUM_MODULES && interval) syncRequest[module] = uint32_t(interval) | (uint32_t(uint16_t(offset)) << 16);
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
  if (capabilityGate[module].test_and_set(std::memory_order_acquire)) return;
  crossfireModuleStatus[module] = status;
  capabilityInvalid[module] = false;
  capabilityGate[module].clear(std::memory_order_release);
  requestModelId(module); // unsolicited device info also indicates module recovery
  auto& config = g_model.moduleData[module].crsf;
  if (!(status.isELRS && status.major >= 4) &&
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
  // A contended lifecycle must be treated as potentially owning the port.
  // Maintenance callers will then stop RF before reconfiguring shared hardware.
  if (portGate[module].test_and_set(std::memory_order_acquire)) return true;
  auto state = modulePortGetState(module);
  bool used = state && state->tx.port && state->tx.port->hw_def == hardware;
  portGate[module].clear(std::memory_order_release);
  return used;
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
