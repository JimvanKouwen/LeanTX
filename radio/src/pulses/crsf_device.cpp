#include "edgetx.h"
#include "rf_internal.h"
#include "telemetry/crsf_device.h"
#include <atomic>

namespace CrsfDevice {
static_assert(MaxPayload + 4 <= CROSSFIRE_FRAME_MAXLEN, "CRSF device frame capacity");
static_assert(std::atomic<uint8_t>::is_always_lock_free, "RF queue must not block");
namespace {
enum : uint8_t { Empty, Writing, Ready, Reading };
std::atomic<uint8_t> state{Empty};
uint8_t pending[CROSSFIRE_FRAME_MAXLEN];
uint8_t pendingSize;
uint8_t pendingModule;
}

bool active()
{
  return moduleState[INTERNAL_MODULE].protocol == PROTOCOL_CHANNELS_CROSSFIRE ||
         moduleState[EXTERNAL_MODULE].protocol == PROTOCOL_CHANNELS_CROSSFIRE;
}

bool available() { return state.load(std::memory_order_acquire) == Empty; }

bool send(uint8_t type, const uint8_t* payload, size_t length)
{
  // Only extended device ping, parameter read and parameter write/command.
  // Commands on parameter fields use 0x2D; arbitrary 0x32 commands are excluded.
  if (!active() || !payload || length < 2 || length > MaxPayload) return false;
  if ((type == 0x28 && length != 2) ||
      (type == 0x2C && length != 4) ||
      (type == 0x2D && length < 4) ||
      (type != 0x28 && type != 0x2C && type != 0x2D)) return false;
  // ELRS 3.x uses the dedicated Lua origin (0xEF) when talking to the
  // TX module. New ELRS and TBS tools use RADIO_ADDRESS. No other origin
  // or destination back to the handset is allowed.
  bool elrsOrigin = payload[1] == 0xEF && payload[0] == MODULE_ADDRESS && type != 0x28;
  if ((!elrsOrigin && payload[1] != RADIO_ADDRESS) ||
      payload[0] == RADIO_ADDRESS || payload[0] == 0xEF ||
      (type != 0x28 && payload[0] == BROADCAST_ADDRESS)) return false;

  uint8_t expected = Empty;
  if (!state.compare_exchange_strong(expected, Writing, std::memory_order_acquire))
    return false;
  pendingModule = moduleState[INTERNAL_MODULE].protocol == PROTOCOL_CHANNELS_CROSSFIRE
                    ? INTERNAL_MODULE : EXTERNAL_MODULE;
  pending[0] = MODULE_ADDRESS;
  pending[1] = length + 2;
  pending[2] = type;
  pending[3] = payload[0];
  pending[4] = elrsOrigin ? 0xEF : RADIO_ADDRESS;
  memcpy(pending + 5, payload + 2, length - 2);
  pending[3 + length] = crc8(pending + 2, length + 1);
  pendingSize = length + 4;
  state.store(Ready, std::memory_order_release);
  return true;
}

size_t take(uint8_t module, uint8_t* frame, size_t capacity)
{
  uint8_t expected = Ready;
  if (!state.compare_exchange_strong(expected, Reading, std::memory_order_acquire))
    return 0;
  if (module != pendingModule || capacity < pendingSize) {
    state.store(Ready, std::memory_order_release);
    return 0;
  }
  size_t size = pendingSize;
  memcpy(frame, pending, size);
  state.store(Empty, std::memory_order_release);
  return size;
}

void cancel()
{
  uint8_t expected = Ready;
  state.compare_exchange_strong(expected, Empty, std::memory_order_acq_rel);
}
}

