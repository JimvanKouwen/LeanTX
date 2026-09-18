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
