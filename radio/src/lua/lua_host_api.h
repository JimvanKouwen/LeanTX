#pragma once

#include <stddef.h>
#include <stdint.h>
#include "edgetx_types.h"

struct ModelData;
struct RadioData;
struct TimerData;
class TelemetryItem;

// Lua -> Core. Read access is const; side effects are explicit operations.
// No Lua types, mutable model references, channel writers or RF setters here.
namespace LuaHostApi {
constexpr size_t DeviceMaxPayload = 60;
const ModelData& model();
const RadioData& radio();
const TelemetryItem& telemetry(unsigned index);
getvalue_t value(mixsrc_t source, bool* valid = nullptr);
int16_t output(unsigned index);
uint8_t switchType(uint8_t index);
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
bool deviceActive();
bool deviceAvailable();
bool deviceSend(uint8_t type, const uint8_t* payload, size_t length);
}
