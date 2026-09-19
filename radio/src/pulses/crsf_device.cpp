#include "edgetx.h"
#include "rf_internal.h"
#include "telemetry/crsf_device.h"
#include <atomic>
#include "tasks/mixer_task.h"

namespace CrsfDevice {
static_assert(MaxPayload + 4 <= CROSSFIRE_FRAME_MAXLEN, "CRSF device frame capacity");
static_assert(std::atomic<uint8_t>::is_always_lock_free, "RF queue must not block");
namespace {
enum : uint8_t { Empty, Writing, Ready, Reading, Cancelled };
std::atomic<uint8_t> state{Empty};
uint8_t pending[CROSSFIRE_FRAME_MAXLEN];
uint8_t pendingSize;
uint8_t pendingModule;
}

bool active()
{
  if (!mixerTaskTryLock()) return false;
  bool result = moduleState[INTERNAL_MODULE].protocol == PROTOCOL_CHANNELS_CROSSFIRE ||
                moduleState[EXTERNAL_MODULE].protocol == PROTOCOL_CHANNELS_CROSSFIRE;
  mixerTaskUnlock();
  return result;
}

bool available() { return state.load(std::memory_order_acquire) == Empty; }

bool send(uint8_t type, const uint8_t* payload, size_t length)
{
  // Only extended device ping, parameter read and parameter write/command.
  // Commands on parameter fields use 0x2D; arbitrary 0x32 commands are excluded.
  if (!payload || length < 2 || length > MaxPayload) return false;
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

  // Select the port and reserve the mailbox before lifecycle teardown can
  // cancel it. Never wait for the RF consumer from a request producer.
  if (!mixerTaskTryLock()) return false;
  uint8_t module = moduleState[INTERNAL_MODULE].protocol == PROTOCOL_CHANNELS_CROSSFIRE
                    ? INTERNAL_MODULE : EXTERNAL_MODULE;
  uint8_t expected = Empty;
  bool reserved = moduleState[module].protocol == PROTOCOL_CHANNELS_CROSSFIRE &&
      state.compare_exchange_strong(expected, Writing, std::memory_order_acquire);
  mixerTaskUnlock();
  if (!reserved) return false;
  pendingModule = module;
  pending[0] = MODULE_ADDRESS;
  pending[1] = length + 2;
  pending[2] = type;
  pending[3] = payload[0];
  pending[4] = elrsOrigin ? 0xEF : RADIO_ADDRESS;
  memcpy(pending + 5, payload + 2, length - 2);
  pending[3 + length] = crc8(pending + 2, length + 1);
  pendingSize = length + 4;
  expected = Writing;
  if (!state.compare_exchange_strong(expected, Ready, std::memory_order_release)) {
    state.store(Empty, std::memory_order_release);
    return false;
  }
  return true;
}

size_t take(uint8_t module, uint8_t* frame, size_t capacity)
{
  uint8_t expected = Ready;
  if (!state.compare_exchange_strong(expected, Reading, std::memory_order_acquire))
    return 0;
  if (module != pendingModule || capacity < pendingSize) {
    expected = Reading;
    if (!state.compare_exchange_strong(expected, Ready, std::memory_order_release))
      state.store(Empty, std::memory_order_release);
    return 0;
  }
  size_t size = pendingSize;
  memcpy(frame, pending, size);
  return state.exchange(Empty, std::memory_order_acq_rel) == Cancelled ? 0 : size;
}

void cancel()
{
  uint8_t expected = state.load(std::memory_order_acquire);
  while (expected == Ready || expected == Writing || expected == Reading) {
    uint8_t next = expected == Ready ? Empty : Cancelled;
    if (state.compare_exchange_weak(expected, next, std::memory_order_acq_rel)) return;
  }
}
}

