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

#include "gtests.h"
#include "pulses/rf_internal.h"
#include "hal/module_driver.h"
#include "hal/module_port.h"
#include "hal/serial_driver.h"
#include "pulses/modules_constants.h"
#include "pulses/pulses.h"
#include "translations/translations.h"

TEST(ports, softserialFallback)
{
  modulePortInit();

  const etx_serial_init serialCfg = {
    .baudrate = 57600,
    .encoding = ETX_Encoding_8N1,
    .direction = ETX_Dir_RX,
    .polarity = ETX_Pol_Inverted,
  };

  bool has_softserial = modulePortFind(EXTERNAL_MODULE, ETX_MOD_TYPE_SERIAL,
                                       ETX_MOD_PORT_SPORT_INV, ETX_Pol_Inverted,
                                       ETX_Dir_RX);
  if (has_softserial) {
    auto mod_st = modulePortInitSerial(EXTERNAL_MODULE, ETX_MOD_PORT_SPORT,
                                       &serialCfg, false);
    EXPECT_TRUE(mod_st == nullptr);

    mod_st = modulePortInitSerial(EXTERNAL_MODULE, ETX_MOD_PORT_SPORT,
                                  &serialCfg, true);
    EXPECT_TRUE(mod_st != nullptr);
    if (!mod_st) return;

    modulePortDeInit(mod_st);
  }
}

#if defined(HARDWARE_EXTERNAL_MODULE)
TEST(ports, isPortUsed)
{
  modulePortInit();

  const etx_serial_init serialCfg = {
    .baudrate = 57600,
    .encoding = ETX_Encoding_8N1,
    .direction = ETX_Dir_TX_RX,
    .polarity = ETX_Pol_Normal,
  };

  auto mod_st = modulePortInitSerial(EXTERNAL_MODULE, ETX_MOD_PORT_SPORT,
                                     &serialCfg, false);
  EXPECT_TRUE(mod_st != nullptr);
  EXPECT_TRUE(mod_st && mod_st->rx.port != nullptr);

  auto module = modulePortGetModuleForPort(ETX_MOD_PORT_SPORT);
  EXPECT_EQ(EXTERNAL_MODULE, module);

  if (mod_st) modulePortDeInit(mod_st);
  EXPECT_FALSE(modulePortIsPortUsed(ETX_MOD_PORT_SPORT));
}
#endif

TEST(ports, RfChannelWindowStaysInsideOutputs)
{
  MODEL_RESET();
  auto driver = pulsesGetModuleDriver(EXTERNAL_MODULE);
  auto savedDriver = *driver;
  auto savedState = moduleState[EXTERNAL_MODULE];
  int calls = 0;
  etx_proto_driver_t probe{};
  probe.txCompleted = [](void*) { return true; };
  probe.sendPulses = [](void* context, uint8_t*, const int16_t* channels, uint8_t count) {
    ++*static_cast<int*>(context);
    EXPECT_EQ(CROSSFIRE_CHANNELS_COUNT, count);
    ASSERT_GE(channels, channelOutputs);
    ASSERT_LE(channels + count, channelOutputs + MAX_OUTPUT_CHANNELS);
    // Read the entire window as an encoder does (also checked by ASan).
    for (unsigned i = 0; i < count; ++i)
      EXPECT_EQ(channels - channelOutputs + i, channels[i]);
  };
  driver->drv = &probe;
  driver->ctx = &calls;
  moduleState[EXTERNAL_MODULE] = {};
  moduleState[EXTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_NONE;
  for (int i = 0; i < MAX_OUTPUT_CHANNELS; ++i) channelOutputs[i] = i;
  for (unsigned start = 0; start <= 255; ++start) {
    g_model.moduleData[EXTERNAL_MODULE].channelsStart = start;
    // A small stored count must not weaken the fixed encoder window bound.
    g_model.moduleData[EXTERNAL_MODULE].channelsCount = -8;
    pulsesSendNextFrame(EXTERNAL_MODULE);
  }
  EXPECT_EQ(256, calls);
  *driver = savedDriver;
  moduleState[EXTERNAL_MODULE] = savedState;
}

#include <atomic>
#include <thread>
#include <future>
#include <chrono>

TEST(ports, BusyTxPreservesOwnedBuffer)
{
  MODEL_RESET();
  auto driver = pulsesGetModuleDriver(EXTERNAL_MODULE);
  auto savedDriver = *driver;
  auto savedState = moduleState[EXTERNAL_MODULE];
  struct Probe { bool busy = false; uint8_t* owned = nullptr; unsigned calls = 0; } state;
  etx_proto_driver_t probe{};
  probe.txCompleted = [](void* ctx) { return !static_cast<Probe*>(ctx)->busy; };
  probe.sendPulses = [](void* ctx, uint8_t* buffer, const int16_t*, uint8_t) {
    auto& p = *static_cast<Probe*>(ctx);
    ++p.calls;
    memset(buffer, p.calls, 64);
    p.owned = buffer;
    p.busy = true;
  };
  *driver = {&probe, &state};
  moduleState[EXTERNAL_MODULE] = {};
  moduleState[EXTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_NONE;
  pulsesSendNextFrame(EXTERNAL_MODULE);
  ASSERT_NE(nullptr, state.owned);
  for (unsigned slot = 0; slot < 1000; ++slot) pulsesSendNextFrame(EXTERNAL_MODULE);
  EXPECT_EQ(1u, state.calls);
  for (unsigned i = 0; i < 64; ++i) EXPECT_EQ(1, state.owned[i]);
  state.busy = false;
  pulsesSendNextFrame(EXTERNAL_MODULE);
  EXPECT_EQ(2u, state.calls);
  *driver = savedDriver;
  moduleState[EXTERNAL_MODULE] = savedState;
}

TEST(ports, TelemetrySnapshotAndLuaContentionDoNotBlockTx)
{
  MODEL_RESET();
  auto driver = pulsesGetModuleDriver(EXTERNAL_MODULE);
  auto savedDriver = *driver;
  auto savedState = moduleState[EXTERNAL_MODULE];
  auto port = modulePortGetState(EXTERNAL_MODULE);
  auto savedPort = *port;
  struct Probe { std::atomic<bool> entered{false}, release{false}; std::atomic<unsigned> sends{0}; } state;
  etx_serial_driver_t serial{};
  serial.copyRxBuffer = [](void* ctx, uint8_t*, uint32_t) {
    auto& p = *static_cast<Probe*>(ctx);
    p.entered = true;
    while (!p.release) std::this_thread::yield();
    return 0;
  };
  etx_module_port_t definition{};
  definition.drv.serial = &serial;
  port->rx = {&definition, &state};
  port->user_data = &state;
  etx_proto_driver_t probe{};
  probe.txCompleted = [](void*) { return true; };
  probe.sendPulses = [](void* ctx, uint8_t*, const int16_t*, uint8_t) {
    ++static_cast<Probe*>(static_cast<etx_module_state_t*>(ctx)->user_data)->sends;
  };
  *driver = {&probe, port};
  moduleState[EXTERNAL_MODULE] = {};
  moduleState[EXTERNAL_MODULE].protocol = PROTOCOL_CHANNELS_NONE;
  std::thread telemetry([&] {
    LuaTelemetryLock luaLock;
    RfService::pollFrame(EXTERNAL_MODULE, &probe);
  });
  while (!state.entered) std::this_thread::yield();
  auto realtime = std::async(std::launch::async, [&] {
    for (unsigned i = 0; i < 1000; ++i) pulsesSendNextFrame(EXTERNAL_MODULE);
  });
  EXPECT_EQ(std::future_status::ready, realtime.wait_for(std::chrono::seconds(1)));
  state.release = true;
  telemetry.join();
  realtime.get();
  EXPECT_EQ(1000u, state.sends.load());
  *port = savedPort;
  *driver = savedDriver;
  moduleState[EXTERNAL_MODULE] = savedState;
}
