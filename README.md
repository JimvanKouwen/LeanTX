# LeanTX RF support

LeanTX supports CRSF modules only: ExpressLRS and TBS Crossfire. Internal
radios with legacy RF hardware default to RF off; use a compatible CRSF module.
Removed or unrecognized module types in model files load as RF off.

Trainer and buddy-box support (including PPM, SBUS, Bluetooth, and CRSF
trainer input) has been removed. CRSF telemetry, Lua configuration, and ELRS
firmware flashing remain supported. RF binding and failsafe settings belong
to the CRSF module/receiver; the radio also offers the ELRS bind shortcut when
supported by the module.

Build the representative matrix with `./build-targets.sh lite`, or all current
firmware presets plus the simulator with `./build-targets.sh full`.

## Sparse canonical YAML storage

Radio settings and models use sparse canonical YAML. Missing fields retain
normal defaults; unknown or unavailable fields are ignored and dropped on save.
Loads read the complete file into a shared 16 KiB buffer, parse sections directly
from RAM, validate an isolated candidate, and commit only on success. Saves
regenerate YAML from runtime state without reading the previous file, omit
defaults and unused slots, and flush `.tmp` before recoverable replacement.

Storage is independent of packed C++ layouts. There are no struct dumps,
per-target storage layouts, or binary caches. See the
[format and transaction documentation](docs/radio-configuration.md).

[![GitHub release (latest by date)](https://img.shields.io/github/v/release/Edgetx/edgetx)](https://github.com/EdgeTX/edgetx/releases/latest)
[![GitHub all releases](https://img.shields.io/github/downloads/EdgeTX/edgetx/total)](https://github.com/EdgeTX/edgetx/releases)
[![GitHub license](https://img.shields.io/github/license/Edgetx/edgetx)](https://github.com/EdgeTX/edgetx/blob/main/LICENSE)
[![Commit Tests](https://github.com/EdgeTX/edgetx/actions/workflows/build_fw.yml/badge.svg)](https://github.com/EdgeTX/edgetx/actions/workflows/build_fw.yml)
[![GitHub CodesSpaces ready-to-code](https://img.shields.io/badge/GitHub%20CodesSpaces-ready--to--code-blue?logo=github)](https://codespaces.new/EdgeTX/edgetx)
[![Conventional Commits](https://img.shields.io/badge/Conventional%20Commits-1.0.0-%23FE5196?logo=conventionalcommits&logoColor=white)](https://conventionalcommits.org)
[![Discord](https://img.shields.io/discord/839849772864503828.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/wF9wUKnZ6H)
[![Support us on OpenCollective](https://img.shields.io/opencollective/all/edgetx)](https://opencollective.com/edgetx)

<p align="center">
<a href="https://raw.githubusercontent.com/EdgeTX/edgetx.github.io/master/docs/assets/logo.png"><img src="https://raw.githubusercontent.com/EdgeTX/edgetx.github.io/master/docs/assets/logo.png" align="center" height="150" width="150" ></a>

# Welcome to EdgeTX!
**The cutting edge open-source firmware for your R/C radio!**

### About EdgeTX
EdgeTX is the cutting edge of OpenTX. It is the place where innovative ideas and cutting-edge features are developed and field-tested by the enthusiasts of our hobby. EdgeTX is a community project – ideas from the community, developed by the community, and enjoyed by the community! The community will always have a say in what EdgeTX is and what EdgeTX will be in the future. Without community feedback and involvement EdgeTX cannot exist.

### Community
- [Discord](https://discord.gg/wF9wUKnZ6H)

- [Facebook](https://www.facebook.com/groups/edgetx)

- [Github Discussions](https://github.com/EdgeTX/edgetx/discussions)

### Navigation Links

- [Community Guidelines](https://github.com/EdgeTX/edgetx.github.io/wiki/Community-Guidlines)

- [Installation Guide](https://manual.edgetx.org/installing-and-updating-edgetx/update-from-opentx-to-edgetx)

- [Installation Video](https://www.youtube.com/watch?v=Y9OvW9XCjOs)

- [Reporting Issues / Requesting features](https://github.com/EdgeTX/edgetx/issues/new/choose)

- [Lua Documentation Site](https://luadoc.edgetx.org/)

- Buddy: [Info](https://github.com/EdgeTX/buddy) - [Downloads](https://github.com/EdgeTX/buddy/releases)

- SD Card: [Info](https://github.com/EdgeTX/edgetx-sdcard) - [Downloads](https://github.com/EdgeTX/edgetx-sdcard/releases)

- Sound Packs:  [Info](https://github.com/EdgeTX/edgetx-sdcard-sounds) - [Downloads](https://github.com/EdgeTX/edgetx-sdcard-sounds/releases)

- [Developer Documentation](https://edgetx.org/edgetx/latest/) - [Docker Build Environment](https://github.com/EdgeTX/build-edgetx)

## Acknowledgements
Some icon assets provided by [ICONS8](https://icons8.com).</br>
Lua Documentation site powered with the kind support of [GitBook](https://www.gitbook.com).

Traditional trim values are removed: physical trim buttons never offset stick
inputs. Each direction remains a momentary switch (`T1-`, `T1+`, and so on),
independent of stick mapping, and can be selected wherever a
switch condition is accepted. GPIO scanning, button diagnostics, simulator
buttons, and bootloader button combinations remain supported. No new button
actions are assigned by default.

Old trim values, inheritance, and trim-specific actions are ignored when loading
models. Remaining packed trim positions and numeric source/action IDs are reserved
to avoid shifting unrelated settings; reserved fields are not written to YAML.
Audio-recording trim tools are unaffected.

Transmitter flight modes are removed, including switch conditions,
mode masks, fades, and mode-specific settings.
Betaflight modes still work through AUX channels; received CRSF mode telemetry
and assignable flight-controller mode sound prompts remain supported.

Global Variables (GVars) are removed, including their storage, editors, special
functions, Lua APIs, and simulator exports. Direct channel mappings have no
weights, offsets, source inversion, or variable references.

These changes break compatibility with old packed model backups and models
using flight modes or GVars. Old mode/variable data is ignored and variable
references are not migrated. Recreate and check affected model settings.
Lua mode and global-variable APIs are no longer available.

Transmitter Logical Switches are removed: no L01–L64 conditions/sources, logical
expressions, sticky/edge/timer evaluation, delay/duration state, monitors, audio
prompts, log columns, or Logical Switch Lua APIs remain. Physical switch positions
and their inversion, trim buttons, multiposition controls, function-switch hardware,
and telemetry availability conditions remain supported. Timers retain switch conditions. Channel mappings read physical switch
positions at native minimum/center/maximum.

This changes packed model layout and source/switch IDs after the removed ranges.
Physical switch position IDs are unchanged. Old logical-switch definitions are
ignored; their references are not migrated. Recreate and verify affected conditions
before using old models (an unknown condition may become an unconditioned field).
No replacement event/condition system has been introduced.

## Special and Global Functions removed

LeanTX no longer stores or evaluates EdgeTX Custom Functions (model Special
Functions or radio Global Functions). Their editors, menu pages, enable flags,
Lua configuration APIs, action constants, scheduling state, and YAML handlers
are removed. No replacement event or binding system is provided.

Old YAML `customFn`, `noGlobalFunctions`, `radioGFDisabled`, and
`modelSFDisabled` entries are ignored and are not written back. Packed model
and radio-settings layouts have changed; old binary backups are incompatible.
Old action configurations are discarded, including channel-override assignments.

### Preserved capabilities

- Normal radio volume and brightness settings and their source controls run
  directly in the UI loop. Backlight timeout, activity, and alarm flashing remain.
- Audio playback, haptic output, screenshots, timer set/reset, flight and
  telemetry reset, module binding, Lua execution, hardware switches, key locking,
  touch hardware, microphone recording, and logging implementations remain.
- `playValue()` is declared in `audio.h` and implemented in `audio_value.cpp`.
- `AudioQueue::setBackgroundPaused()` controls background playback directly;
  queued background files play without a function-active bit. Flushing audio
  clears the pause state.
- `varioWakeup()` remains callable with telemetry/FAI guards. Automatic vario
  wakeups are removed so deleting functions does not silently enable vario audio.
- Logging remains callable; a nonzero `logDelay100ms` enables the existing logger.
  Its default is zero. There is currently no user-facing logging trigger.

Trainer support was already removed earlier in LeanTX; this change does not
restore it.

### Actions without their former entry points

There is no longer switch-triggered/repeating sound, haptic, spoken-value,
background-music/pause, screenshot, timer/reset, bind, backlight/volume override,
channel-override, screen-selection, key/touch-disable, amplifier-mute, video-input,
or customizable-switch-push action. Existing direct system/UI/Lua routes remain
where provided. In particular, standalone/tools, telemetry and widgets remain; function-scheduled scripts and RGB script scheduling are gone.
Logging, automatic vario activation and function script scheduling need future
user-facing entry points. The old Racing Mode action had no evaluator dispatch.

### Audit and future work

Production code contains no Custom Function configuration or evaluation symbols.
The removed names remain only in regression tests and this compatibility note.
`FUNCTION_SWITCHES`, `FUNCTION_SWITCHES_RGB_LEDS`, their fields and switch/LED
APIs refer to physical customizable switches and remain. `PLAY_FUNCTION` and
`I18N_PLAY_FUNCTION` declare reusable speech functions. Sensor-register `FUNC_*`
and vendor function symbols are unrelated. `AU_SPECIAL_SOUND_*` identifies
reusable audio/haptic patterns and remains.

Possible later simplifications, deliberately deferred: consolidate the remaining
switch-selection contexts; design explicit logging/vario controls; expose the
preserved actions through Values/Events; review startup-audio timing and unused
view-option padding; simplify now-sparser menus. Physical values pass directly to RF through channel mappings.

### Validation

Pocket firmware and the native simulator build successfully. Additional
X9D+ 2019 and TX16S MK3 firmware builds cover the wider monochrome and color
interfaces. The simulator runs 54 selected model/storage/Lua/switch/timer/haptic
regression tests and the control tests successfully. Configuration compatibility uses checked-in YAML fixtures. Hardware flight testing remains
outside these build and simulator checks.

## Standalone RTC Emergency Mode

RTC recovery now uses explicit radio/control/CRSF snapshots, independent of the
YAML schema and packed runtime layouts. Field-by-field capture and restore replace
`BACKUP`/`NOBACKUP` alternate structures and generated copy helpers. Calibration,
Channel mappings, physical controls and module settings are retained;
removed-feature reservations, timers and unrelated display/configuration data are
excluded. Input filtering and HAL flex-switch mappings are also preserved.

The 4,096-byte RTC area holds a 20-byte header and an RLC-compressed snapshot.
Magic, format version, build/target tag, lengths, CRC-32 and strict decompression
must all pass before runtime state changes. Failed boot recovery leaves RF off.
The existing dirty-mask and one-second backup timing are unchanged. Pocket has
no enabled hardware RTC-backup path; the simulator can exercise it with
`-DRTC_BACKUP_RAM=YES`.

See [the RTC field audit and validation report](docs/rtc-emergency.md) for retained
fields, exclusions, sizes, test results and remaining limitations. YAML formats
and the normal startup path are unchanged.

## Configuration storage

Radio, model, metadata, labels, and theme files use semantic section adapters.
See [sparse canonical YAML storage](docs/radio-configuration.md) for the limited
YAML subset, defaults, compatibility aliases, 16 KiB file limit, and replacement
protocol. Rejected model loads block autosaves and list metadata updates for
that file, so the previous active model cannot overwrite it.

## Direct physical channel mapping

The temporary control path is **physical input → channel mapping →
`channelOutputs[]` → RF**. Each channel has one `ChannelMapping` containing a
`PhysicalInputId`; its array index is the destination channel. Duplicate channel
mappings are therefore impossible. Every update assigns the normalized physical
value directly, including all integer values in the native -1024…1024 range.
Unassigned or unavailable inputs produce zero without retaining previous output.

HAL/ADC/GPIO, calibration, hardware inversion, filtering, deadzone and physical
switch decoding remain. RF normalization and packet clamping remain in the encoder.
There are no weights, offsets, constant or channel sources, source inversion,
additive mappings, throttle reverse, overrides, mixer passes, or output caches.
Generic `mixsrc_t`/`getValue()` APIs remain for UI/audio/Lua and system values only.
Lua mixer configuration APIs and the old Mixer editors have been removed.

The channel mapping UI selects one physical source (or none) per channel.
YAML uses zero-based channel and physical-input indices:

```yaml
channelMappings:
  0:
    source: stick(3)  # Ail → CH1
  1:
    source: stick(1)  # Ele → CH2
  2:
    source: stick(2)  # Thr → CH3
  3:
    source: stick(0)  # Rud → CH4
  4:
    source: switch(0)
```

`flex(n)` selects a physical pot/slider; `NONE` clears a channel. Stick indices
follow the existing axis identities and stick-mode hardware decoding. New models
retain the configured channel order, with AETR as the radio default. Old `mixData`
and `throttleReversed` YAML is ignored and omitted on save; recreate mappings for
old models. No legacy arithmetic is migrated. Duplicate channel definitions and
nonphysical mapping sources are rejected. RTC snapshots use version 7; older
snapshots cannot be restored. No Variables, transforms, Events, middleware, or
future Value system has been added.

### Lua application scripting

Lua remains available for tools/apps, widgets, telemetry scripts and telemetry
access, including CRSF/ELRS tools and the LCD, audio and filesystem APIs.
Legacy `/SCRIPTS/MIXES` scripts, their model configuration and input/output tables,
and Lua mixer sources (`LUA1a`, etc.) are removed. Lua scripts no longer contribute
outputs to the realtime mixer/source graph. There is no replacement control API.
Old mix-script configuration is ignored when loading YAML and omitted on save;
references to Lua output sources such as `lua(0,0)` are rejected as invalid sources.

The Inputs/ExpoData stage has been removed. New radio defaults use AETR (Ail → CH1, Ele → CH2,
Thr → CH3, Rud → CH4), respecting the configured channel order for new models.
Old models using `I0`/`I1` sources require manual adjustment; there is no migration.

The legacy Curves subsystem is removed from model storage, YAML, Lua, and both
UIs. Physical calibration and normalization remain intact. No replacement transform system is introduced. Retired YAML fields are
ignored on load and omitted on save.

The Outputs/LimitData subsystem is removed: endpoint limits, subtrim, channel
reverse, symmetrical limits, PPM center adjustment, channel names, editors,
Lua output configuration, and transient output overrides are gone. Mapped values
pass directly into `channelOutputs[]` in native physical-input units. Physical input calibration/normalization remains intact; CRSF conversion
and packet-range clamping happen only in the protocol encoder. Channel monitors
remain read-only. Legacy Outputs YAML is ignored on load and omitted on save.
RTC snapshots use version 7; older snapshots cannot be restored.
