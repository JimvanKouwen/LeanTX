# Lua boundaries

Core execution and input delivery enter `LuaRuntime`. It delegates to the
existing interpreter, monochrome tool runner, color standalone window and
widget implementations. These implementations stay in their current files;
`standalone_lua.cpp` is part of the Lua adapter despite its GUI directory.
The runtime also owns the existing telemetry queues. It adds no interpreter,
virtual dispatch or allocation beyond the existing implementation.

Bindings use `LuaHostApi` for const model/radio/telemetry observations, source
values, output inspection, audio/haptic requests, timer operations, cosmetic
model metadata, backlight/screenshot/LED operations and device requests.
There is no mutable model accessor or RF/channel writer on this interface.
Drawing and native GUI resources remain in the existing Lua UI adapter.
Telemetry publication, sensor/warning setters, files, resource lifetimes and
execution budgets remain later work, as do Values/Events.

## CRSF device service

`CrsfDevice` is a core service implemented alongside the CRSF transmitter.
`crossfireTelemetryPush` is a compatibility byte-table parser, not a raw-frame
injection API. Core validates and constructs only:

| Type | Operation | Payload length including destination/origin |
| --- | --- | --- |
| 0x28 | Device discovery | 2 |
| 0x2C | Parameter/chunk read | 4 |
| 0x2D | Parameter write or parameter command | 4–60 |

0x32 raw commands and all other frame types, including 0x16/0x17 channel frames,
are rejected. Parameter commands (start, confirm, cancel, poll) use 0x2D.
Broadcast is discovery-only. Directed requests target devices, never the
handset. Core accepts origin 0xEA, plus the ELRS 3.x 0xEF origin only when
addressing the TX module with a parameter request. Core constructs the
transport address, length and CRC. Internal CRSF takes precedence over external
CRSF, matching the old binding. Incoming device responses retain their original
payloads through `LuaRuntime` and `crossfireTelemetryPop`.

A single fixed 64-byte slot has lock-free acquire/release publication. Rejected
requests cannot publish partial data. The RF scheduler consumes this slot only
after a normal channel frame and must transmit another channel frame before
consuming another device request. Startup model-ID/discovery and binding remain
core-owned. Module lifecycle and runtime shutdown cancel pending requests.
The old raw telemetry output buffer is not consumed by the CRSF transmitter.

Compatibility traces in the tests cover the current
[ExpressLRS tool](https://github.com/ExpressLRS/Lua/blob/master/elrs.lua) and
[ELRS 3.x](https://github.com/ExpressLRS/ExpressLRS/blob/3.x.x-maintenance/src/lua/elrsV3.lua):
broadcast discovery, chunk reads, writes, command polling and both handset
origins. Tests also cover responses, invalid types/bytes/lengths, full-size
frames, queue atomicity, stack stability and sustained channel transmission.

`model.setInfo` accepts name and bitmap only; `jitterFilter` remains readable.
`model.setModule` is absent; `model.getModule` remains available. RF type,
channel windows and arming configuration must be changed by core-owned UI or
configuration operations.

## Validation of this batch

- Pocket firmware, Pocket simulator and TX16S MK3 color firmware build.
- Pocket's complete native suite passes: 184 tests, including the Lua and
  CRSF/device compatibility and scheduling regressions.
- A separate Debug Pocket native build with `-fsanitize=address` on both C and
  C++ sources passes 32 CRSF/control tests (excluding Lua tests).
- Fully instrumented Lua startup hits an existing global-buffer-over-read in
  `thirdparty/Lua/src/ltable.c:787`: the ROTable lookup reads four bytes from the
  three-byte `_G` string. No third-party source was changed in this batch.
  Recompiling only that translation unit with `-fno-sanitize=address` allows all
  32 Lua/CRSF tests to pass while the bindings, runtime and device service remain
  instrumented. This is a limited sanitizer result, not a clean fully
  instrumented Lua run. Leak checking was disabled for these runs.

Build and test logs for this workspace are under
`builds/lua-boundary-validation/`. Compatibility validation uses protocol traces;
no on-device RF hardware run was performed.
