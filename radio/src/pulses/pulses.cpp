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

void pulsesInit()
{
  memset(_module_drivers, 0, sizeof(_module_drivers));
}

module_pulse_driver* pulsesGetModuleDriver(uint8_t module)
{
  return &(_module_drivers[module]);
}

uint8_t* pulsesGetModuleBuffer(uint8_t module)
{
  return _module_buffers[module]._buffer;
}

ModuleState moduleState[NUM_MODULES];

void pulsesStart()
{
  telemetryStart();
  mixerTaskStart();
}

void pulsesStop()
{
  telemetryStop();
  mixerTaskStop();

  for (uint8_t i = 0; i < MAX_MODULES; i++)
    pulsesStopModule(i);
}

void restartModule(uint8_t module)
{
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
bool restartModuleAsync(uint8_t module, uint8_t cnt_delay)
{
  void* param1 = (void*)(uintptr_t)module;
  return async_call(_setup_async_module_restart,
                    &_module_restart_queued[module], param1, cnt_delay);
}

void pulsesModuleSettingsUpdate(uint8_t module)
{
  moduleState[module].settings_updated = 1;
}





ModuleSettingsMode getModuleMode(int moduleIndex)
{
  return (ModuleSettingsMode)moduleState[moduleIndex].mode;
}

void setModuleMode(int moduleIndex, ModuleSettingsMode mode)
{
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

void pulsesStopModule(uint8_t module)
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
    int16_t* channels = &channelOutputs[channelStart];
    uint8_t nChannels = CROSSFIRE_CHANNELS_COUNT;

    auto buffer = _module_buffers[module]._buffer;
    drv->sendPulses(ctx, buffer, channels, nChannels);
  }
}

void pulsesSendChannels()
{
  for (uint8_t i = 0; i < MAX_MODULES; i++) {
    pulsesSendNextFrame(i);
  }
}
