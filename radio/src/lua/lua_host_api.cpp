#include "edgetx.h"
#include "lua_host_api.h"
#include "telemetry/crsf_device.h"
#if defined(LED_STRIP_GPIO)
#include "boards/generic_stm32/rgb_leds.h"
#include "hal/rgbleds.h"
#endif

namespace LuaHostApi {
static_assert(DeviceMaxPayload == CrsfDevice::MaxPayload, "Device payload capacity");
uint8_t switchType(uint8_t index) { return g_model.getSwitchType(index); }
void flushAudio() { audioQueue.flush(); }
const ModelData& model() { return g_model; }
const RadioData& radio() { return g_eeGeneral; }
const TelemetryItem& telemetry(unsigned index) { return telemetryItems[index]; }
getvalue_t value(mixsrc_t source, bool* valid) { return getValue(source, valid); }
int16_t output(unsigned index) { return index < MAX_OUTPUT_CHANNELS ? channelOutputs[index] : 0; }
void setModelName(const char* name)
{
  strncpy(g_model.header.name, name, sizeof(g_model.header.name));
  storageDirty(EE_MODEL);
}
void setModelBitmap(const char* name)
{
#if LCD_DEPTH > 1
  strncpy(g_model.header.bitmap, name, LEN_BITMAP_NAME);
  storageDirty(EE_MODEL);
#endif
}
void setTimer(unsigned index, const TimerData& timer, int32_t value, bool updateValue)
{
  if (index >= MAX_TIMERS) return;
  g_model.timers[index] = timer;
  if (updateValue) timersStates[index].val = value;
  storageDirty(EE_MODEL);
}
int32_t timerValue(unsigned index) { return index < MAX_TIMERS ? timersStates[index].val : 0; }
void resetTimer(unsigned index) { if (index < MAX_TIMERS) timerReset(index); }
void resetGlobalTimer(const char* option)
{
  if (!strcmp(option, "all") || !strcmp(option, "total")) {
    g_eeGeneral.globalTimer = 0;
    sessionTimer = 0;
  } else if (!strcmp(option, "session")) {
    sessionTimer = 0;
  }
  storageDirty(EE_GENERAL);
}
void playFile(const char* file, uint8_t flags, uint8_t id, int8_t volume)
{ audioQueue.playFile(file, flags, id, volume); }
void playNumber(int32_t number, uint8_t unit, uint8_t flags, uint8_t id, int8_t volume)
{ ::playNumber(number, unit, flags, id, volume); }
void playDuration(int32_t value, uint8_t flags, uint8_t id, int8_t volume)
{ ::playDuration(value, flags, id, volume); }
void playTone(uint16_t frequency, uint16_t length, uint16_t pause, uint8_t flags, int8_t increment, int8_t volume)
{ audioQueue.playTone(frequency, length, pause, flags, increment, volume); }
void playHaptic(uint8_t length, uint8_t pause, uint8_t flags, uint8_t intensity)
{
#if defined(HAPTIC)
  haptic.play(length, pause, flags, intensity);
#endif
}
void screenshot() { writeScreenshot(); }
void refreshDisplay()
{
#if !defined(COLORLCD)
  lcdRefresh();
#endif
}
void resetBacklight() { resetBacklightTimeout(); }
void setLedColor(uint8_t id, uint8_t r, uint8_t g, uint8_t b)
{
#if defined(LED_STRIP_GPIO)
  rgbSetLedColor(id, r, g, b);
#endif
}
void applyLedColors()
{
#if defined(LED_STRIP_GPIO)
  rgbLedColorApply();
#endif
}
void setFunctionLed(unsigned id, bool enabled, uint8_t r, uint8_t g, uint8_t b)
{
#if defined(CFS_LED_STRIP_LENGTH) && CFS_LED_STRIP_LENGTH > 0
  setFSLedOverride(id, enabled, r, g, b);
#endif
}
bool deviceActive() { return CrsfDevice::active(); }
bool deviceAvailable() { return CrsfDevice::available(); }
bool deviceSend(uint8_t type, const uint8_t* payload, size_t length)
{ return CrsfDevice::send(type, payload, length); }
}
