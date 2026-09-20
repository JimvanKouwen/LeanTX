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

#include "edgetx.h"
#include "os/async.h"
#include "os/timer.h"
#include "mixer_scheduler.h"
#include "hal/module_port.h"
#include "sensor_names.h"

#include <list>

#if !defined(SIMU)
  #include <FreeRTOS/include/FreeRTOS.h>
  #include <FreeRTOS/include/timers.h>
#endif

#if defined(CROSSFIRE)
  #include "crossfire.h"
#endif

struct telemetry_buffer {
  uint8_t buffer[TELEMETRY_RX_PACKET_SIZE];
  uint8_t length;
};

uint8_t telemetryStreaming = 0;
uint8_t telemetryState = TELEMETRY_INIT;

TelemetryData telemetryData;
static rxStatStruct rxStat;

telemetry_buffer _telemetry_rx_buffer[NUM_MODULES];

uint8_t* getTelemetryRxBuffer(uint8_t moduleIdx)
{
  return _telemetry_rx_buffer[moduleIdx].buffer;
}

uint8_t &getTelemetryRxBufferCount(uint8_t moduleIdx)
{
  return _telemetry_rx_buffer[moduleIdx].length;
}

rxStatStruct *getRxStatLabels() {
  rxStat.label = STR_RXSTAT_LABEL_RQLY;
  rxStat.unit = STR_RXSTAT_UNIT_PERCENT;
  rxStat.max = 100;
  return &rxStat;
}

static void (*telemetryMirrorSendByte)(void*, uint8_t) = nullptr;
static void* telemetryMirrorSendByteCtx = nullptr;

void telemetrySetMirrorCb(void* ctx, void (*fct)(void*, uint8_t))
{
  telemetryMirrorSendByte = nullptr;
  telemetryMirrorSendByteCtx = ctx;
  telemetryMirrorSendByte = fct;
}

void telemetryMirrorSend(uint8_t data)
{
  auto _sendByte = telemetryMirrorSendByte;
  auto _ctx = telemetryMirrorSendByteCtx;

  if (_sendByte) {
    _sendByte(_ctx, data);
  }
}

static timer_handle_t telemetryTimer = TIMER_INITIALIZER;

static void telemetryTimerCb(timer_handle_t* h)
{
  DEBUG_TIMER_START(debugTimerTelemetryWakeup);
  telemetryWakeup();
  DEBUG_TIMER_STOP(debugTimerTelemetryWakeup);
}

void telemetryStart()
{
  if (!timer_is_created(&telemetryTimer)) {
    timer_create(&telemetryTimer, telemetryTimerCb, "Telem", 2, true);
  }

  timer_start(&telemetryTimer);
}

void telemetryStop()
{
  if (timer_is_created(&telemetryTimer)) {
    timer_stop(&telemetryTimer);
  }
}

static volatile bool _poll_frame_queued[NUM_MODULES] = {false};

static void _poll_frame(void *pvParameter1, uint32_t ulParameter2)
{

  auto drv = (const etx_proto_driver_t*)pvParameter1;
  auto module = (uint8_t)ulParameter2;
  _poll_frame_queued[module] = false;

  RfService::pollFrame(module, drv);

}

void telemetryFrameTrigger_ISR(uint8_t module, const etx_proto_driver_t* drv)
{
  async_call_isr(_poll_frame, &_poll_frame_queued[module], (void*)drv, module);
}


void telemetryWakeup()
{
  static bool firstSwitchPoll = true;
  getSwitchesPosition(firstSwitchPoll);
  firstSwitchPoll = false;
  processPhysicalInputSounds();
  doMixerPeriodicUpdates();
#if defined(IMU)
  gyroWakeup();
#endif
#if defined(BLUETOOTH)
  bluetooth.wakeup();
#endif
  for (uint8_t i = 0; i < MAX_MODULES; i++) {
    RfService::pollTelemetry(i);
  }

  for (int i = 0; i < MAX_TELEMETRY_SENSORS; i++) {
    const TelemetrySensor& sensor = g_model.telemetrySensors[i];
    if (sensor.type == TELEM_TYPE_CALCULATED) {
      telemetryItems[i].eval(sensor);
    }
  }


  static tmr10ms_t alarmsCheckTime = 0;
#define SCHEDULE_NEXT_ALARMS_CHECK(seconds) \
  alarmsCheckTime = get_tmr10ms() + (100 * (seconds))
  if (int32_t(get_tmr10ms() - alarmsCheckTime) > 0) {
    SCHEDULE_NEXT_ALARMS_CHECK(1 /*second*/);

    bool sensorLost = false;
    for (int i = 0; i < MAX_TELEMETRY_SENSORS; i++) {
      if (isTelemetryFieldAvailable(i)) {
        TelemetryItem& item = telemetryItems[i];
        if (item.timeout == 0) {
          TelemetrySensor* sensor = &g_model.telemetrySensors[i];
          if (sensor->unit != UNIT_DATETIME) {
            item.setOld();
            sensorLost = true;
          }
        }
      }
    }

    if (sensorLost && TELEMETRY_STREAMING() &&
        !g_model.disableTelemetryWarning) {
      audioEvent(AU_SENSOR_LOST);
    }

    if (!g_model.disableTelemetryWarning) {
      if (TELEMETRY_STREAMING()) {
        if (TELEMETRY_RSSI() < g_model.rfAlarms.critical) {
          AUDIO_RSSI_RED();
          SCHEDULE_NEXT_ALARMS_CHECK(10 /*seconds*/);
        } else if (TELEMETRY_RSSI() < g_model.rfAlarms.warning) {
          AUDIO_RSSI_ORANGE();
          SCHEDULE_NEXT_ALARMS_CHECK(10 /*seconds*/);
        }
      }

      if (TELEMETRY_STREAMING()) {
        if (telemetryState == TELEMETRY_INIT) {
          AUDIO_TELEMETRY_CONNECTED();
        } else if (telemetryState == TELEMETRY_KO) {
          AUDIO_TELEMETRY_BACK();

#if defined(CROSSFIRE)
          // TODO: move to crossfire code
#if defined(HARDWARE_EXTERNAL_MODULE)
          if (isModuleCrossfire(EXTERNAL_MODULE)) {
            RfService::requestModelId(EXTERNAL_MODULE);
          }
#endif

#if defined(HARDWARE_INTERNAL_MODULE)
          if (isModuleCrossfire(INTERNAL_MODULE)) {
            RfService::requestModelId(INTERNAL_MODULE);
          }
#endif
#endif
        }
        telemetryState = TELEMETRY_OK;
      } else if (telemetryState == TELEMETRY_OK) {
        telemetryState = TELEMETRY_KO;
        if (!isModuleInBeepMode()) {
          AUDIO_TELEMETRY_LOST();
        }
      }
    }
  }
}

void telemetryInterrupt10ms()
{
  if (telemetryStreaming > 0) {
    bool tick160ms = (telemetryStreaming & 0x0F) == 0;
    for (int i=0; i<MAX_TELEMETRY_SENSORS; i++) {
      const TelemetrySensor & sensor = g_model.telemetrySensors[i];
      if (sensor.type == TELEM_TYPE_CALCULATED) {
        telemetryItems[i].per10ms(sensor);
      }
      if (tick160ms && telemetryItems[i].timeout > 0) {
        telemetryItems[i].timeout--;
      }
    }
    telemetryStreaming--;
  }
  else {
#if !defined(SIMU)
    telemetryData.rssi.reset();
#endif
    for (auto & telemetryItem: telemetryItems) {
      if (telemetryItem.isAvailable()) {
        telemetryItem.setOld();
      }
    }
  }
}

void telemetryReset()
{
  telemetryData.clear();

  for (auto & telemetryItem : telemetryItems) {
    telemetryItem.clear();
  }

  telemetryStreaming = 0; // reset counter only if valid telemetry packets are being detected
  telemetryState = TELEMETRY_INIT;
}

#if defined(LOG_TELEMETRY) && !defined(SIMU)
extern FIL g_telemetryFile;
void logTelemetryWriteStart()
{
  static tmr10ms_t lastTime = 0;
  tmr10ms_t newTime = get_tmr10ms();
  if (lastTime != newTime) {
    struct gtm utm;
    gettime(&utm);
    f_printf(&g_telemetryFile, "\r\n%4d-%02d-%02d,%02d:%02d:%02d.%02d0:",
             utm.tm_year + TM_YEAR_BASE, utm.tm_mon + 1, utm.tm_mday,
             utm.tm_hour, utm.tm_min, utm.tm_sec, g_ms100);
    lastTime = newTime;
  }
}

void logTelemetryWriteByte(uint8_t data)
{
  f_printf(&g_telemetryFile, " %02X", data);
}
#endif

OutputTelemetryBuffer outputTelemetryBuffer __DMA_NO_CACHE;
