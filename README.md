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

## Radio-settings storage redesign

Radio settings, models, model headers, labels, and themes use the streaming
configuration engine. The legacy generated struct-reflection system has been
removed; storage no longer relies on C++ field offsets or packed struct sizes.
The engine, in
`radio/src/storage/config_stream.h`/`.cpp`, treats `radio.yml` purely as a
configuration document:

- A single universal field table lists every known radio setting once,
  independent of PCB/target. Hardware availability is decided separately
  per field via a small capability predicate (e.g. IMU-only fields), not by
  `#ifdef`-ing settings out of the schema.
- Fields are classified at load/save time as known+available,
  known+unavailable, missing+relevant, or unknown, per the rules in
  `config_stream.h`. Unknown and known-but-unavailable data (including
  nested maps/lists) is always preserved byte-for-byte across saves without
  ever being parsed into memory.
- Loading and saving are bounded-memory streaming operations: fixed-size
  line buffers only, no YAML DOM, no heap growth with file size.
- Saves write to `radio_new.yml`, then replace `radio.yml` only after the
  temporary file is fully written, so a failed save never touches the
  existing file.
- A simple schema version (`RADIO_CONFIG_SCHEMA_VERSION`) is reserved for
  future semantic migrations; ordinary field additions/removals need no
  version bump.

The schema now covers essentially all plain scalar/bitfield `RadioData`
settings (roughly 60 fields: calibration targets, RF/haptic/audio/display
preferences, RTC/timezone, per-target capability-gated fields such as
`contrast`/`radioThemesDisabled`/`imuMax`/`stickDeadZone`, and two-letter
language codes), plus a nested example: `switches:` persists each switch's
type as an individually known+available/unavailable element (by hardware
switch count), while unrecognised per-switch attributes (e.g. a custom
`name`) are preserved unchanged.

Still out of scope for this pass, and intentionally left as preserved
"unknown" data rather than silently dropped: array/custom-encoded settings
that need dedicated adapters (`sticksConfig`, `slidersConfig`,
`potsConfig`, `serialPort`, analog `calib[]`, `flexSwitches`, color-LCD
`keyShortcuts`/`qmFavorites`), and a handful of string-tag-encoded legacy
fields (`semver`, `board`, `telemetryBaudrate`, `jitterFilter`,
`auxSerialMode`/`aux2SerialMode`, `rotEncDirection`). These are documented
follow-up work for the same engine. RTC Emergency Mode snapshot/restore is
unrelated and was not touched.

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
Ordinary output offsets and audio-recording trim tools are unaffected.

Transmitter flight modes are removed. Inputs and Mixes retain their ordinary
switch conditions, but have no mode masks, fades, or mode-specific settings.
Betaflight modes still work through AUX channels; received CRSF mode telemetry
and assignable flight-controller mode sound prompts remain supported.

Global Variables (GVars) are removed, including their storage, editors, special
functions, Lua APIs, and simulator exports. Input and mixer weights/offsets,
output limits/offsets, and curve parameters now hold literal numbers only.
Source selection for Inputs and Mixes remains supported; numeric settings no
longer encode references to sources or variables.

These changes break compatibility with old packed model backups and models
using flight modes or GVars. Old mode/variable data is ignored and variable
references are not migrated. Recreate and check affected model settings.
Lua mode and global-variable APIs are no longer available.

Transmitter Logical Switches are removed: no L01–L64 conditions/sources, logical
expressions, sticky/edge/timer evaluation, delay/duration state, monitors, audio
prompts, log columns, or Logical Switch Lua APIs remain. Physical switch positions
and their inversion, trim buttons, multiposition controls, function-switch hardware,
and telemetry availability conditions remain supported. Inputs, Mixes, and timers
retain their ordinary switch conditions.

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
- `setChannelOverride(channel, percent, enabled)` retains direct transient
  output override behavior. It bypasses ordinary output limits. Passing `false`
  disables that channel's override; `clearChannelOverrides()` clears all of them.
  Loading a model clears overrides. There are no switch assignments or stored
  override configurations.
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
where provided. In particular, standalone/tools, telemetry, mixer scripts and
widgets remain; function-scheduled scripts and RGB script scheduling are gone.
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
view-option padding; simplify now-sparser menus. Inputs, Mixes, and Outputs keep
their existing engine and ordinary switch conditions.

### Validation

Pocket firmware and the native simulator build successfully. Additional
X9D+ 2019 and TX16S MK3 firmware builds cover the wider monochrome and color
interfaces. The simulator runs 54 selected model/storage/Lua/switch/timer/haptic
regression tests and all 28 mixer tests successfully (the override test is in both
runs). Configuration compatibility uses checked-in YAML fixtures. Hardware flight testing remains
outside these build and simulator checks.

## Standalone RTC Emergency Mode

RTC recovery now uses explicit radio/control/CRSF snapshots, independent of the
YAML schema and packed runtime layouts. Field-by-field capture and restore replace
`BACKUP`/`NOBACKUP` alternate structures and generated copy helpers. Calibration,
Inputs/Mixes/Outputs/curves, physical controls and module settings are retained;
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

## Universal radio configuration

Radio settings now use a universal semantic schema and bounded streaming YAML
load/merge, independent of generated layouts and packed `RadioData` offsets.
Known available settings update runtime state; missing settings receive normal
defaults. Unknown content and known settings unavailable on the current radio
survive normal saves. Existing dirty/debounce and forced-flush behavior remains.

Saves complete and flush `radio.yml.tmp` before replacement, with a transient
swap file for rename recovery. No ordinary backup files are created. Model YAML
uses the streaming model path described below; RTC emergency recovery is unchanged.

Pocket, TX16S MK3 and simulator builds pass, alongside 105 monochrome and 106
color regression tests and 18 sanitizer-tested core cases. See the
[radio configuration report](docs/radio-configuration.md) for APIs, capabilities,
YAML limits, transaction guarantees, and measured flash/RAM costs (3908 bytes
of static file workspace on Pocket; 4096-byte compile-time ceiling).

### Streaming model YAML storage

Model files now use the same bounded streaming config engine as `radio.yml`,
with a universal semantic schema and model-specific adapters. Loads initialize a
candidate, parse once, resolve telemetry/screen unions, and commit only after
successful validation and I/O. Normal saves merge existing YAML, preserving
unknown subtrees and settings unavailable on the current radio, add missing
applicable defaults, and replace the file through `.tmp` with `.swap` recovery.
Inputs, mixes, outputs, curves, timers, telemetry, Lua configuration, modules,
metadata, and screens/widgets retain their existing runtime representations.
Model selection reads only metadata and module types without constructing a
full model. Compatibility tests use checked-in legacy YAML fixtures; the
generated descriptors and generator have been removed.

Rejected model loads display an error and block autosaves and list metadata
updates for that file, so the previous active model cannot overwrite it.
