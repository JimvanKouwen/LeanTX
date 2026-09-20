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
// Non-critical consumer of coalesced analog-centre sound requests.
void processPhysicalInputSounds();

// Physical switch conditions have their own namespace, with no inversion or
// generic value sources. Positions: 0 = up, 1 = middle, 2 = down.
constexpr int PHYSICAL_SWITCH_CONDITION_MAX = 96;
constexpr uint8_t physicalSwitchCondition(unsigned index, unsigned position)
{ return index < 32 && position < 3 ? 1 + index * 3 + position : 0; }
bool isPhysicalSwitchConditionAvailable(int condition);
bool readPhysicalSwitchCondition(uint8_t condition);
