# RF runtime boundary

`RfService` is the static RF facade. Its implementation and fixed storage live in
`pulses.cpp`; `pulses.h` keeps small lifecycle compatibility entry points for board
and core callers. `rf_internal.h` belongs only to this directory and white-box
RF tests. There is no public transmit buffer, driver context, encoder channel
argument, mutable capability object, or device-queue consumer API.

The mixer still generates `PhysicalInput -> ChannelMapping -> channelOutputs`.
`RfService::sendChannels()` reads a bounded window of those final outputs and
passes a **const** view to the CRSF encoder. It does not write channel outputs.
Model storage remains the source of configuration; RF owns runtime counters,
driver contexts, DMA buffers, module capabilities, sync feedback and CRSF slot
eligibility. Capability queries return snapshots.

Lua and UI device requests use `CrsfDevice` (`telemetry/crsf_device.h` is the
compatibility include path). Its fixed mailbox accepts only validated extended
ping and parameter operations. Only the RF scheduler can consume it. A device
frame requires a preceding normal channel slot, cannot occupy consecutive slots,
and cannot take priority over bind or model-ID/discovery traffic. Device requests
cannot submit raw frames or RC channel values. The encoder retains the existing
model-ID recovery, discovery, physical-switch arming and timing behavior.

Telemetry keeps its decoding, values and receive queues. Receive-side adapters
hide RF driver access; decoded sync and device-info messages update RF through
explicit methods. Capability discovery and arming capability fallback do not
depend on Lua being compiled or running.

No allocations or virtual interfaces are introduced in the RF path.

Validation: run `python3 tools/check_rf_boundary.py` from the repository root and
run the simulator `gtests-radio` target. `PhysicalControlRfTest`, `Crossfire`,
`ports.RfChannelWindowStaysInsideOutputs`, and the Lua device-service tests cover
channel provenance, arming, sustained device traffic, malformed requests and
capability discovery.

HAL port implementation and board initialization remain below the RF boundary.
Firmware flashing and CLI serial passthrough temporarily own the ports after
stopping RF, and resume RF afterward. These maintenance paths are separate from
runtime device requests; telemetry's unused legacy output buffer is not an RF
transmit source. The serial configuration code queries port use through RF, and
CLI power/boot-pin operations validate module indexes through RF.

Safety scheduling contract: initialization sends the current model ID first.
Thereafter every management frame (including discovery, model ID and bind)
requires a preceding RC frame. Sync periods are clamped to
850–50000 us; an uninitialized/expired sync uses the baud-rate default. Each
transmit also budgets its actual UART wire time plus 100 us. The completion gate
prevents reuse of the module buffer while DMA/IRQ TX owns it. With two modules,
the global scheduler can tick before the other module finishes; the completion
gate skips these ticks. At 115200 baud a maximum 64-byte frame occupies at most
5556 us. With scheduler intervals at most 50000 us, two successful TX slots are
bounded by 112 ms, excluding task/interrupt latency and hardware faults. A single
module with completed TX sends RC at least every second scheduled slot (100 ms
at the maximum configured period). These are software bounds, not measured RF
on-air latency; verify actual UART timing and reconnect behavior on hardware.

The one-frame device mailbox rejects new requests when occupied. Cancellation
also invalidates an in-progress producer or consumer without releasing storage
until that owner finishes. Script teardown cancels queued commands and clears
old replies. Color queue registration/removal and Lua packet extraction share
the RF receive lock. Extended responses follow the same internal-module-first
selection as requests; CRSF parameter IDs/chunks remain the tool's correlation
fields (the wire protocol has no transaction token).
