#pragma once

// Private RF implementation state. Only pulses/ and white-box tests include this.
#include "pulses.h"
#include "telemetry/crossfire.h"
PACK(struct ModuleState {
  uint8_t protocol;
  uint8_t mode:4;
  uint8_t forced_off:1;
  uint8_t settings_updated:1;
  uint8_t spare:2;
  uint16_t counter;

});

extern ModuleState moduleState[NUM_MODULES];

#define CROSSFIRE_FRAME_MAXLEN 64

#define MODULE_BUFFER_SIZE 64
static_assert(MODULE_BUFFER_SIZE >= CROSSFIRE_FRAME_MAXLEN, "RF TX buffer capacity");

struct module_pulse_buffer {
  uint8_t _buffer[MODULE_BUFFER_SIZE];
};

struct module_pulse_driver {
  const etx_proto_driver_t* drv;
  void* ctx;
};

module_pulse_driver* pulsesGetModuleDriver(uint8_t module);

extern CrossfireModuleStatus crossfireModuleStatus[NUM_MODULES];
namespace CrsfDevice { size_t take(uint8_t module, uint8_t* frame, size_t capacity); }

// Module pulse synchronization
struct ModuleSyncStatus
{
  // feedback input: last received values
  uint16_t  refreshRate; // in us
  int16_t   inputLag;    // in us

  tmr10ms_t lastUpdate;  // in 10ms
  int16_t   currentLag;  // in us

  inline bool isValid() const {
    // 2 seconds
    return refreshRate != 0 && (tmr10ms_t)(get_tmr10ms() - lastUpdate) < 200;
  }

  // Set feedback from RF module
  void update(uint16_t newRefreshRate, int16_t newInputLag);

  //mark as timeouted
  void invalidate();

  // Get computed settings for scheduler
  uint16_t getAdjustedRefreshRate();

  // Status string for the UI
  void getRefreshString(char* refreshText);

  ModuleSyncStatus();
};

ModuleSyncStatus& getModuleSyncStatus(uint8_t moduleIdx);

void pulsesSendNextFrame(uint8_t module);

typedef void (*module_init_cb_t)(uint8_t, const etx_proto_driver_t*);
typedef void (*module_deinit_cb_t)(uint8_t, const etx_proto_driver_t*);

void pulsesSetModuleInitCb(module_init_cb_t cb);
void pulsesSetModuleDeInitCb(module_deinit_cb_t cb);

// Re-Init module
// 
// Note: this can only be used from within
//       module init.
void pulsesRestartModuleUnsafe(uint8_t module);

