#pragma once

#include <stddef.h>
#include <stdint.h>
#include "edgetx_types.h"

struct ModelData;
struct RadioData;
struct TimerData;
class TelemetryItem;
struct TelemetrySensor;

// Lua -> Core. Read access is const; side effects are explicit operations.
namespace LuaHostApi {
constexpr size_t RawCrsfMaxPayload = 60;
const ModelData& model();
const RadioData& radio();
const TelemetryItem& telemetry(unsigned index);
getvalue_t value(mixsrc_t source, bool* valid = nullptr);
int16_t output(unsigned index);
uint8_t switchType(uint8_t index);
bool publishTelemetry(uint16_t id, uint8_t subId, uint8_t instance, int32_t value,
                      uint32_t unit, uint32_t prec, const char* name);
uint8_t switchWarning(uint8_t index);
void setSwitchWarning(uint8_t index, uint8_t state);
const TelemetrySensor& sensor(unsigned index);
void resetSensor(unsigned index);
int luaSerialPort();
void setSerialBaudrate(int port, uint32_t baudrate);
bool serialPower(uint8_t port, bool& enabled);
bool setSerialPower(uint8_t port, uint8_t enabled);
void setIMU_X(int16_t offset, int16_t range);
void setIMU_Y(int16_t offset, int16_t range);
void flushAudio();
void setModelName(const char* name);
void setModelBitmap(const char* name);
void setTimer(unsigned index, const TimerData& timer, int32_t value, bool updateValue);
int32_t timerValue(unsigned index);
void resetTimer(unsigned index);
void resetGlobalTimer(const char* option);
void playFile(const char* file, uint8_t flags, uint8_t id, int8_t volume);
void playNumber(int32_t number, uint8_t unit, uint8_t flags, uint8_t id, int8_t volume);
void playDuration(int32_t value, uint8_t flags, uint8_t id, int8_t volume);
void playTone(uint16_t frequency, uint16_t length, uint16_t pause, uint8_t flags, int8_t increment, int8_t volume);
void playHaptic(uint8_t length, uint8_t pause, uint8_t flags, uint8_t intensity);
void screenshot();
void refreshDisplay();
void resetBacklight();
void setLedColor(uint8_t id, uint8_t r, uint8_t g, uint8_t b);
void applyLedColors();
void setFunctionLed(unsigned id, bool enabled, uint8_t r, uint8_t g, uint8_t b);
// Trusted Lua may send any CRSF type and payload, including channel frames.
size_t rawCrsfPayloadCapacity(uint8_t type);
bool rawCrsfActive();
bool rawCrsfAvailable();
bool rawCrsfSend(uint8_t type, const uint8_t* payload, size_t length);
}
