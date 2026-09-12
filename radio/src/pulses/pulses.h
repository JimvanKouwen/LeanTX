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

#pragma once

#include "definitions.h"
#include "dataconstants.h"
#include "pulses_common.h"
#include "hal/module_driver.h"

PACK(struct ModuleState {
  uint8_t protocol;
  uint8_t mode:4;
  uint8_t forced_off:1;
  uint8_t settings_updated:1;
  uint8_t spare:2;
  uint16_t counter;

});

extern ModuleState moduleState[NUM_MODULES];

inline bool isModuleBeeping(uint8_t moduleIndex)
{

  return moduleState[moduleIndex].mode == MODULE_MODE_BIND;
}

template<class T> struct PpmPulsesData {
  T pulses[20];
  T * ptr;
};

#define PPM_DEF_PERIOD               225 /* 22.5ms */
#define PPM_STEP_SIZE                5 /*0.5ms*/
#define PPM_PERIOD_FL_TO_HALF_US(fl) (((fl)*PPM_STEP_SIZE+PPM_DEF_PERIOD)*200) /* half us*/
#define PPM_TRAINER_PERIOD_HALF_US() PPM_PERIOD_FL_TO_HALF_US(g_model.trainerData.frameLength) /*half us*/
#define SBUS_BAUDRATE                100000

#define CROSSFIRE_FRAME_MAXLEN         64

union TrainerPulsesData {
  PpmPulsesData<trainer_pulse_duration_t> ppm;
};

extern TrainerPulsesData trainerPulsesData;

  #define MODULE_BUFFER_SIZE 64

struct module_pulse_buffer {
  uint8_t _buffer[MODULE_BUFFER_SIZE];
};

struct module_pulse_driver {
  module_pulse_buffer buffer;
  const etx_proto_driver_t* drv;
  void* ctx;
};

module_pulse_driver* pulsesGetModuleDriver(uint8_t module);
uint8_t* pulsesGetModuleBuffer(uint8_t module);

void pulsesStopModule(uint8_t module);
void pulsesSendNextFrame(uint8_t module);
void pulsesSendChannels();

typedef void (*module_init_cb_t)(uint8_t, const etx_proto_driver_t*);
typedef void (*module_deinit_cb_t)(uint8_t, const etx_proto_driver_t*);

void pulsesSetModuleInitCb(module_init_cb_t cb);
void pulsesSetModuleDeInitCb(module_deinit_cb_t cb);

void restartModule(uint8_t module);
bool restartModuleAsync(uint8_t module, uint8_t cnt_delay);

// Re-Init module
// 
// Note: this can only be used from within
//       module init.
void pulsesRestartModuleUnsafe(uint8_t module);

void pulsesModuleSettingsUpdate(uint8_t module);

void setupPulsesPPMTrainer();

void getModuleStatusString(uint8_t moduleIdx, char * statusText);
void getModuleSyncStatusString(uint8_t moduleIdx, char * statusText);

void pulsesInit();
void pulsesStart();
void pulsesStop();

inline bool isModuleInBeepMode()
{
  if (moduleState[0].mode == MODULE_MODE_BIND)
    return true;

#if NUM_MODULES > 1
  if (moduleState[1].mode == MODULE_MODE_BIND)
    return true;
#endif

  return false;
}
