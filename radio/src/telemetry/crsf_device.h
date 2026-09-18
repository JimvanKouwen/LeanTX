#pragma once

#include <stddef.h>
#include <stdint.h>

// Core-owned CRSF device requests. No raw frame or channel transmission API.
namespace CrsfDevice {
constexpr size_t MaxPayload = 60; // 64-byte frame minus address, length, type, CRC
bool available();
bool active();
bool send(uint8_t type, const uint8_t* payload, size_t length);
void cancel();
}
