# RF runtime boundary and concurrency

`RfService` is the concrete RF facade. `rf_internal.h`, module contexts, fixed
TX buffers and the device-mailbox consumer belong to RF and its white-box tests.
The channel path remains physical input → `PhysicalInputId` → `ChannelMapping`
→ `channelOutputs[]` → protocol encoding. Lua cannot supply a channel frame.
CH5 arming is retained. Alternate arming accepts physical switch positions only;
missing/invalid conditions and function switches are disarmed.

Every management transmission (model selection, discovery, bind, or a validated
Lua device request) consumes one scheduling credit. Only an RC frame replenishes
it. Consequently there is at most one management frame between RC frames, even
under discovery, bind, model-selection and device-request floods. With no pending
management, every available slot sends RC. Initial model selection precedes the
first RC frame. Discovery and Lua requests alternate management opportunities
while capabilities are unknown; bind and model selection have priority.

The driver must report TX complete before the RF buffer is touched. Busy DMA or
IRQ transmission skips the slot without consuming scheduling credit or the
mailbox. The STM32 single-buffer IRQ adapter also checks ownership before changing
its pointer. No new TX buffer, allocation, or virtual interface is involved.

| Primitive | Protected state and behavior |
| --- | --- |
| Lock-free atomic mailbox state | One fixed 64-byte device request. Producer claims Empty once or fails. RF claims Ready once or skips. Release/acquire publication prevents torn frames. Cancellation exchanges to Cancelled; an active writer/reader must release storage before reuse. A committed take cannot be recalled. |
| Per-module atomic fields/mailboxes | Protocol/mode/settings, restart request, model-selection request, packed interval/offset sync feedback, receive liveness, partial reset and receiver link state. Sync pairs are published together and consumed by RF. |
| Per-module capability try gate | Short bounded capability struct copy only. Contended publication drops; contended reads return an empty snapshot. Storage fallback work runs after release. |
| Per-module UART snapshot try gate | RX copies at most `TELEMETRY_RX_PACKET_SIZE` bytes to the stack, then releases UART ownership before logging or parsing. Normal TX never takes this gate. |
| Per-module parser try gate | Serializes partial-buffer access and parsing. Lifecycle must also claim it so a previous module's parser cannot update a replacement module. Normal TX never takes it. Realtime lifecycle work defers if occupied. |
| Dedicated Lua telemetry mutex | Queue publication, list registration/removal, delivery, clearing and pop copies. Deregistration completes before destruction. Lua table allocation follows release. RF never takes this mutex. |
| Atomic analog-sound bitmask | Coalesces centre sounds; telemetry wakeup consumes them. Switch sounds, timers/audio, Bluetooth and IMU wakeup also run outside the control task. |
| Mixer mutex | Only the realtime owner and `mixerTaskStop()` acquire it. Stop intentionally pauses RF. No telemetry, Lua, UI, logging, queue or storage operation holds it. There are no callers of the retained try-lock API. |

Normal RF transmission never waits for Lua or telemetry. Explicit synchronous
stop/deinit/restart can wait for an in-progress parser/snapshot after pausing the
control task; that is an intentional lifecycle interruption. Asynchronous restart
uses a one-entry atomic request, and the realtime consumer defers lifecycle work
on contention. Serial passthrough/flashing remain explicit port lifecycle users.

Initialization resets sync/capability state and requests the selected model ID.
A valid frame after a >500 ms absence triggers model selection again; device-info
replies also request selection, covering modules that announce a faster reset.
Repeated recovery follows the same scheduling rule. Bind/unbind routing uses
recent receiver link quality for the selected module, not global telemetry from
another module. Invalid/unsupported module types remain off; failed initialization
is retried. Stored baud indexes fall back safely and sync periods stay bounded.

CRSF reassembly retains at most 63 partial bytes in fixed storage and accepts
wire frames of 4–64 bytes. It checks headers, lengths and CRC, handles arbitrarily
split input chunks without discarding the next chunk's tail, and discards partials
older than 100 ms. A new module resets partial state in the parser itself.
Telemetry scalar/array decoders also check declared field bounds; text is copied
to a terminated local buffer. Signed wire decoding uses unsigned shifts.

Validation commands:

```
python3 tools/check_rf_boundary.py
cmake --build builds/build-lua-asan --target gtests-radio -j6
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  builds/build-lua-asan/gtests-radio \
  --gtest_filter='Crossfire.*:PhysicalControlRfTest.*:PhysicalControlTest.*:ports.*:Lua*.*:Timers.*:ModelAudio*.*'
cmake --build builds/build-tx16smk3/native --target gtests-radio -j6
builds/build-tx16smk3/native/gtests-radio \
  --gtest_filter='Crossfire.*:PhysicalControlRfTest.*:PhysicalControlTest.*:ports.*:Lua*.*:Timers.*'
./build-targets.sh --target pocket --target simu --target tx16smk3
```

Tests include all frame sizes and every two-chunk split, stale/malformed recovery,
management flooding, UART buffer preservation, concurrent cancellation, Lua
shutdown, queue lifecycle contention, repeated module recovery, bind routing,
sync/baud limits, physical arming, function-switch rejection and TX progress while
telemetry and Lua are held. The Lua sanitizer run also exposed an unaligned,
out-of-bounds four-byte ROM-key comparison; it now uses bounded string comparison.

Hardware verification still needs UART/DMA/IRQ timing under load, electrical
brownouts (especially resets with no detectable RX gap or announcement), actual
ELRS/TBS discovery/parameter/bind flows, and reconnection at each supported baud.
Host tests establish software ownership and scheduling, not over-the-air timing
or DMA cache coherency on every board.

Final validation on the `teardown` base `b47943e7d` (2026-09-20): Pocket
ASan/UBSan 68/68, TX16S MK3 color ASan 67/67, boundary check passed, and Pocket
firmware, Pocket simulator and TX16S MK3 firmware all built. Logs are in
`builds/rf-concurrency-validation/`. An additional color audio filename round-trip
test fails at `model_audio.cpp:79`, including when run alone; it is outside the
RF suite and was left unchanged. Leak detection was disabled for the host runs.
The workspace branch changed externally to `rfreview2`, which has the same HEAD
as `teardown`; implementation changes remain uncommitted.
