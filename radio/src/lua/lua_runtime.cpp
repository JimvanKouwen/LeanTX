#include "edgetx.h"
#include "tasks/mixer_task.h"
#include "lua_runtime.h"
#include "lua_event.h"
#include "telemetry/crsf_device.h"
#if defined(COLORLCD)
#include "standalone_lua.h"
#include "lua_widget.h"
#endif

namespace LuaRuntime {
void initialize() { luaInitMainState(); }
void shutdown() { CrsfDevice::cancel(); luaClose(); }
bool run(bool allowLcd) { return luaTask(allowLcd); }
void execute(const char* filename) {
#if defined(COLORLCD)
  luaExecStandalone(filename);
#else
  luaExec(filename);
#endif
}
void receiveEvent(event_t event) { luaPushEvent(event); }
void receiveSerial(uint8_t* data, uint32_t length) { luaReceiveData(data, length); }
void setSerialSend(void* context, void (*send)(void*, uint8_t)) { luaSetSendCb(context, send); }
void setSerialRead(void* context, int (*read)(void*, uint8_t*)) { luaSetGetSerialByte(context, read); }
void allocateSerialBuffer() { luaAllocRxFifo(); }
void freeSerialBuffer() { luaFreeRxFifo(); }
#if defined(COLORLCD)
void collectWidgetGarbage() { luaDoGc(lsWidgets, false); }
void unregisterWidgets() { luaUnregisterWidgets(); }
void runStandalone(StandaloneLuaWindow& window) { window.runCallback(); }
void createWidget(LuaWidget& widget, int function, const char* path) { widget.createCallback(function, path); }
void initializeWidgets() { luaInitThemesAndWidgets(); }
void executeStandalone(const char* filename) { luaExecStandalone(filename); }
void updateWidget(LuaWidget& widget) { widget.updateCallback(); }
void refreshWidget(LuaWidget& widget, BitmapBuffer* display) { widget.refreshCallback(display); }
void backgroundWidget(LuaWidget& widget) { widget.backgroundCallback(); }
#endif
}

#if defined(LUA)
TelemetryQueue* luaInputTelemetryFifo = nullptr;
#if defined(COLORLCD)
std::list<TelemetryQueue*> telemetryQueues;

void registerTelemetryQueue(TelemetryQueue* queue)
{
  mixerTaskLock();
  telemetryQueues.emplace_back(queue);
  mixerTaskUnlock();
}

void deregisterTelemetryQueue(TelemetryQueue* queue)
{
  mixerTaskLock();
  telemetryQueues.remove(queue);
  mixerTaskUnlock();
}
#endif

static void pushDataToQueue(TelemetryQueue* queue, uint8_t* data, int length)
{
  if (length > 0 && length <= LUA_TELEMETRY_INPUT_FIFO_SIZE && queue && queue->hasSpace(length)) {
    for (int i = 0; i < length; i += 1) {
      queue->push(data[i]);
    }
  }
}

void LuaRuntime::receiveTelemetry(uint8_t* data, int length)
{
#if defined(COLORLCD)
  for (auto it = telemetryQueues.cbegin(); it != telemetryQueues.cend(); ++it)
    pushDataToQueue(*it, data, length);
#endif
  pushDataToQueue(luaInputTelemetryFifo, data, length);
}
#endif
