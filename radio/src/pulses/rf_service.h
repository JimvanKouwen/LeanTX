#pragma once
#include <stddef.h>
#include <stdint.h>
#include "pulses_common.h"

struct etx_proto_driver_t;
constexpr uint8_t CRSF_NAME_MAXSIZE = 16;
struct CrossfireModuleStatus
{
    uint8_t major;
    uint8_t minor;
    uint8_t revision;
    char name[CRSF_NAME_MAXSIZE];
    bool queryCompleted;
    bool isELRS;
};

// RF owns all transmit state. Callers never supply RC values or transmit buffers.
namespace RfService {
void init();
void start();
void stop();
void stopModule(uint8_t module);
void restart(uint8_t module);
bool restartAsync(uint8_t module, uint8_t delay);
void settingsChanged(uint8_t module);
// Board/serial integration and diagnostic controls; never expose port contexts.
bool usesTxHardware(uint8_t module, const void* hardware);
bool setModulePower(int module, bool enabled);
bool setBootPin(int module, bool enabled);
ModuleSettingsMode mode(int module);
void setMode(int module, ModuleSettingsMode mode);
void sendChannels();
bool active(uint8_t module);
void requestModelId(uint8_t module);
void beginDiscovery(uint8_t module);
CrossfireModuleStatus capabilities(uint8_t module);
bool elrsVersionAtLeast(uint8_t module, uint8_t major, uint8_t minor);
void receiveDeviceInfo(uint8_t module, const uint8_t* frame, size_t length);
void updateSync(uint8_t module, uint16_t interval, int16_t offset);
// Receive-side adapters keep driver contexts inside RF; telemetry retains decoding.
void pollFrame(uint8_t module, const etx_proto_driver_t* expected);
void pollTelemetry(uint8_t module);
}
