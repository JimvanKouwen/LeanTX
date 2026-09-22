#include "edgetx.h"
#include "lua_host_api.h"
#include "telemetry/crossfire.h"
#if defined(LED_STRIP_GPIO)
#include "boards/generic_stm32/rgb_leds.h"
#include "hal/rgbleds.h"
#endif

namespace LuaHostApi {
static_assert(RawCrsfMaxPayload + 4 == TELEMETRY_OUTPUT_BUFFER_SIZE, "Raw CRSF capacity");
uint8_t switchType(uint8_t index) { return g_model.getSwitchType(index); }
uint8_t switchWarning(uint8_t index) { return g_model.getSwitchWarning(index); }
void setSwitchWarning(uint8_t index, uint8_t state) { g_model.setSwitchWarning(index, state); }
const TelemetrySensor& sensor(unsigned index) { return g_model.telemetrySensors[index]; }
void resetSensor(unsigned index) { if (index < MAX_TELEMETRY_SENSORS) telemetryItems[index].clear(); }
bool publishTelemetry(uint16_t id, uint8_t subId, uint8_t instance, int32_t value,
                      uint32_t unit, uint32_t prec, const char* name)
{
  if (!(id | subId | instance)) return false;
  int index = setTelemetryValue(PROTOCOL_TELEMETRY_LUA, id, subId, instance,
                                value, unit, prec);
  if (index < 0) return false;
  TelemetrySensor& sensor = g_model.telemetrySensors[index];
  sensor.id = id;
  sensor.subId = subId;
  sensor.instance = instance;
  sensor.init(name, unit, prec);
  storageDirty(EE_MODEL);
  return true;
}
int luaSerialPort() { return serialGetModePort(UART_MODE_LUA); }
void setSerialBaudrate(int port, uint32_t baudrate) { ::serialSetBaudrate(port, baudrate); }
bool serialPower(uint8_t port, bool& enabled)
{
#if defined(AUX_SERIAL)
  if (port == SP_AUX1) { enabled = serialGetPower(SP_AUX1); return true; }
#endif
#if defined(AUX2_SERIAL)
  if (port == SP_AUX2) { enabled = serialGetPower(SP_AUX2); return true; }
#endif
  return false;
}
bool setSerialPower(uint8_t port, uint8_t enabled)
{
  if (enabled >= 2) return false;
#if defined(AUX_SERIAL)
  if (port == SP_AUX1) { serialSetPower(SP_AUX1, enabled); return true; }
#endif
#if defined(AUX2_SERIAL)
  if (port == SP_AUX2) { serialSetPower(SP_AUX2, enabled); return true; }
#endif
  return false;
}
void setIMU_X(int16_t offset, int16_t range)
{
#if defined(IMU)
  gyroSetIMU_X(offset, range);
#endif
}
void setIMU_Y(int16_t offset, int16_t range)
{
#if defined(IMU)
  gyroSetIMU_Y(offset, range);
#endif
}
void flushAudio() { audioFlush(); }
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
  if (updateValue) timerSetValue(index, value);
  storageDirty(EE_MODEL);
}
int32_t timerValue(unsigned index) { return index < MAX_TIMERS ? timerGetValue(index) : 0; }
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
{ audioPlayFile(file, flags, id, volume); }
void playNumber(int32_t number, uint8_t unit, uint8_t flags, uint8_t id, int8_t volume)
{ ::playNumber(number, unit, flags, id, volume); }
void playDuration(int32_t value, uint8_t flags, uint8_t id, int8_t volume)
{ ::playDuration(value, flags, id, volume); }
void playTone(uint16_t frequency, uint16_t length, uint16_t pause, uint8_t flags, int8_t increment, int8_t volume)
{ audioPlayTone(frequency, length, pause, flags, increment, volume); }
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
size_t rawCrsfPayloadCapacity(uint8_t type)
{
  return TELEMETRY_OUTPUT_BUFFER_SIZE - (type == COMMAND_ID ? 5 : 4);
}
bool rawCrsfActive()
{
  return moduleState[INTERNAL_MODULE].protocol == PROTOCOL_CHANNELS_CROSSFIRE ||
         moduleState[EXTERNAL_MODULE].protocol == PROTOCOL_CHANNELS_CROSSFIRE;
}
bool rawCrsfAvailable() { return outputTelemetryBuffer.isAvailable(); }
bool rawCrsfSend(uint8_t type, const uint8_t* payload, size_t length)
{
  if (!rawCrsfActive() || !rawCrsfAvailable() ||
      length > rawCrsfPayloadCapacity(type) || (length && !payload)) return false;

  // Construct completely before publishing the destination to the transmitter.
  uint8_t frame[TELEMETRY_OUTPUT_BUFFER_SIZE];
  size_t size = length + (type == COMMAND_ID ? 5 : 4);
  frame[0] = MODULE_ADDRESS;
  frame[1] = size - 2;
  frame[2] = type;
  if (length) memcpy(frame + 3, payload, length);
  if (type == COMMAND_ID) frame[size - 2] = crc8_BA(frame + 2, length + 1);
  frame[size - 1] = crc8(frame + 2, size - 3);
  memcpy(outputTelemetryBuffer.data, frame, size);
  outputTelemetryBuffer.size = size;
  bool internal = moduleState[INTERNAL_MODULE].protocol == PROTOCOL_CHANNELS_CROSSFIRE;
  outputTelemetryBuffer.setDestination(internal ? 0 : TELEMETRY_ENDPOINT_SPORT);
  return true;
}
}
