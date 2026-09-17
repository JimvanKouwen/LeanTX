// Normalized physical controls. Independent of the UI/Lua value namespace.
#pragma once
#include <stdint.h>

enum class PhysicalInputId : uint8_t { None = 0 };
// Stable, disjoint hardware families; indices are zero based.
constexpr PhysicalInputId physicalStick(unsigned index) { return index < 32 ? PhysicalInputId(1 + index) : PhysicalInputId::None; }
constexpr PhysicalInputId physicalFlex(unsigned index) { return index < 32 ? PhysicalInputId(33 + index) : PhysicalInputId::None; }
constexpr PhysicalInputId physicalSwitch(unsigned index) { return index < 32 ? PhysicalInputId(65 + index) : PhysicalInputId::None; }
bool isPhysicalInputAvailable(PhysicalInputId source);
int16_t readPhysicalInput(PhysicalInputId source);

// The array index is the RF channel: exactly one source slot per channel.
struct ChannelMapping {
  PhysicalInputId source;
};
void updateChannelOutputs();
