#pragma once

#include <stdint.h>
#include "edgetx_types.h"

class LuaWidget;
class StandaloneLuaWindow;
class BitmapBuffer;

// Core -> Lua. Lifecycle, execution and input delivery enter here.
namespace LuaRuntime {
void initialize();
void shutdown();
bool run(bool allowLcd);
void execute(const char* filename);
void receiveEvent(event_t event);
void receiveTelemetry(uint8_t* data, int length);
void receiveSerial(uint8_t* data, uint32_t length);
void setSerialSend(void* context, void (*send)(void*, uint8_t));
void setSerialRead(void* context, int (*read)(void*, uint8_t*));
void allocateSerialBuffer();
void freeSerialBuffer();
#if defined(COLORLCD)
void initializeWidgets();
void unregisterWidgets();
void runStandalone(StandaloneLuaWindow& window);
void createWidget(LuaWidget& widget, int function, const char* path);
void collectWidgetGarbage();
void executeStandalone(const char* filename);
void updateWidget(LuaWidget& widget);
void refreshWidget(LuaWidget& widget, BitmapBuffer* display);
void backgroundWidget(LuaWidget& widget);
#endif
}
