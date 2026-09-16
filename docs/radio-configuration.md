# Universal radio configuration

`radio.yml` is a configuration document with explicit semantic bindings to
`g_eeGeneral`. Radio persistence no longer uses generated descriptors, bit
offsets, `YamlNode`, `yaml_bits`, layout annotations, or `sizeof(RadioData)`.
The packed runtime representation remains in place. Model YAML still uses the
legacy generated system. RTC emergency recovery is unchanged and independent.

## Implementation and behavior

- `config_stream.{h,cpp}` provides the bounded streaming parser/merge engine:
  `process`, `Schema`, `Stream`, `Workspace`, and strict scalar conversions in
  the reusable `config_stream` namespace. This module has no radio dependencies.
- `radio_config_fields.inc` declares the universal scalar schema.
- `radio_config_adapter.{h,cpp}` provides `radioSettingsSchema`, explicit runtime
  conversions, named control bindings, capability checks, and legacy aliases.
- `radio_config_file.{h,cpp}` owns `loadRadioSettingsYaml`, `loadRadioSettings`,
  `storageReadRadioSettings`, and `writeGeneralSettings`.

The schema includes settings even when the current target cannot bind them.
HAL/target capabilities decide availability separately:

| Field category | Load | Normal save |
| --- | --- | --- |
| Known and available | Validate and apply | Write runtime value |
| Known but unavailable | Recognize without applying | Preserve source value |
| Missing and relevant | Keep normal default; mark dirty | Insert value |
| Unknown | Ignore at runtime | Preserve source subtree |

Loading initializes a private candidate to normal radio defaults and parses the
file exactly once. The candidate includes auxiliary analog labels, flex-switch
assignments, tool names and configuration-version metadata as well as RadioData.
After successful EOF and file close, dependent fields are resolved and only then
committed. Malformed syntax, duplicate recognized fields, read/close errors, or
failed structural validation discard the candidate and leave live state
untouched. Invalid scalar values still retain defaults and mark the configuration
dirty; unknown and unavailable content keeps the existing preservation semantics.
Existing malformed files and model files are not erased. A complete valid
document does not become dirty merely by being loaded.

No field needs hardware configuration to be applied before parsing. HAL names
and physical capabilities can be read without changing runtime state. Calibration
mid/spans and multiposition count/steps are staged separately; the final pot type
selects the representation, relevant missing/invalid fields, and whether the
steps container must be a mapping. Flex-switch assignments and backlight/volume
sources are resolved against final control types. Serial modes and legacy aliases
are validated against candidate ports after EOF in document order, preserving
first-valid-assignment conflict handling. Serial power callbacks and auxiliary
name updates occur only at commit. The parser and YAML/save format are unchanged.

The existing dirty mask, debounce, shutdown/model-transition forced flushes and
storage task ownership remain in place; saves are not added to the mixer task.

Saving streams the existing document into `radio.yml.tmp`. It never retains the
whole document. Unknown maps, sequences, and unavailable fields pass through;
missing relevant fields are inserted in their containing maps. Recognized
values are explicitly formatted from runtime state.

## Schema and compatibility

`configVersion: 1` identifies semantic interpretation, not a memory layout.
Future positive versions (through 65535) are preserved rather than downgraded.
New fields normally need only a default; removed fields remain preserved unknown
content. Future semantic changes can use explicit migrations without layout
version machinery.

Bindings cover calibration, sticks, pots/sliders, switches (including RGB state),
serial ports, flex switches, sources, color shortcuts/favorites, and scalar radio
settings. Hardware counts and feature-presence flags are not written as facts.
Control keys use HAL names. Sources use names such as `P1` and `LIGHT`; inverted
names are quoted. Calibration uses mid/span values or explicit multiposition
count/steps, not serialized overlay bytes. Existing battery units, volume
normalization and pitch scaling are interpreted explicitly by the adapter.

Straightforward older keys remain readable: `telemetryBaudrate`, `jitterFilter`,
`rotEncDirection`, `auxSerialMode`, `aux2SerialMode`, and `slidersConfig`.
Aliases are not inserted into new files. Legacy calibration stick names and
numeric indices are recognized. Removed features remain unknown and preserved;
old packed multiposition calibration bytes are not reinterpreted as the new
explicit representation.

### Capability decisions

| Capability | Settings affected |
| --- | --- |
| Physical controls | Calibration, pot/slider and switch configuration, flex mappings, source choices |
| Monochrome/color display | Contrast/current-model index versus color layout, themes, labels and favorites |
| Backlight hardware | Brightness (excluding OLED), key lighting, selectable backlight color |
| Audio/haptic | Sound and vibration settings |
| IMU, hats, encoder | IMU limits/offset/inversion, hats mode, encoder mode |
| RTC, Bluetooth, USB charging | Clock options, Bluetooth options, charge control |
| RF and serial hardware | Internal module options, serial port modes/power, UART sampling |
| Input/display implementation | Stick dead zone, LCD inversion |

Difficult cases are user-configurable control types versus physical existence,
calibration overlays, legacy slider names, color-only auxiliary strings, and
serial modes. Physical existence comes from HAL; user control configuration
remains persistent. `NONE` is a valid serial value even when UI choices hide it.
An unsupported mode on an existing port is treated as an invalid value and
falls back safely; an unavailable port's fields are preserved. Availability is
therefore field-based, not a promise to preserve every unsupported enum value.

## Supported YAML profile and limits

The parser accepts block mappings and sequences, nested unknown maps/lists,
indentless sequences, quoted strings with escaped Unicode, single-line flow
collections in unknown content, and literal/folded block scalars. Known schema
containers use block mappings. Unknown data is passed through rather than
normalized into a document tree.

Limits are explicit: 1023 bytes per line, 255 bytes per path, 24 nesting levels,
1024 schema fields and a 128-byte formatted scalar buffer. Individual settings
have smaller type/string bounds. Color tool names have fixed 64-byte slots
(63 bytes plus terminator), replacing auxiliary heap strings outside RadioData.

Aliases, anchors/tags, directives, multiple documents, complex keys, multiline
flow collections and flow-style known containers are outside this profile.
Duplicate recognized scalar fields and exceeded bounds fail cleanly. These
restrictions also apply inside unknown content: unsupported syntax prevents a
save, preserving the active file rather than silently dropping data.

## Filesystem transaction

The replacement must successfully write, flush (`f_sync`) and close before any
rename of the active file. Short writes and source/temporary-file errors abort.
FatFs cannot rename over an existing destination, so replacement uses:

1. Complete `radio.yml.tmp`.
2. Rename active `radio.yml` to `radio.yml.swap`.
3. Rename the completed temporary file to `radio.yml`.
4. Remove the transient swap file.

A failed second rename rolls back; startup restores the swap if the active file
is absent. If rollback itself fails, the original remains in the swap and the
error identifies it. There are no ordinary `.backup` files. The swap is a
transaction recovery file, not a user backup. FAT metadata writes are not atomic
against power loss or torn sectors; this scheme covers interruption between
successful operations, not arbitrary filesystem corruption.

## RAM and flash

No document-size-dependent allocation is used by the configuration engine or
adapter. One serialized static file workspace contains the parser state, two
FatFs file objects, file-info scratch, a 128-byte read buffer and a 512-byte write
buffer. Compile-time assertions cap the core workspace at 2048 bytes and the
complete file workspace at 4096 bytes. The final Pocket object measures the
complete workspace at **2956 bytes**. A busy guard rejects overlapping calls.

The transactional candidate has its own 4096-byte compile-time ceiling and is
static, not allocated on the menus-task stack. The Pocket build uses 504 bytes
for this candidate, in addition to the 2956-byte file workspace. Combined actual
static storage is 3460 bytes; the two working-state ceilings total 8192 bytes,
excluding call stack.
There is no document-size-dependent RAM growth or heap allocation.

Parser depth checks and existing menus-task stack reservations are unchanged.
The flash deltas below predate the transactional loader; no new hardware
stack high-water measurement was taken.

Original radio-configuration migration firmware sizes
(`arm-none-eabi-size`, same configured builds before/after):

| Target | Text before → after | Data before → after | BSS before → after | Flash delta (text + data) |
| --- | ---: | ---: | ---: | ---: |
| Pocket | 388236 → 402684 | 584 → 588 | 47428 → 50388 | +14452 bytes |
| TX16S MK3 | 1323856 → 1338028 | 1212 → 1216 | 11404468 → 11408148 | +14176 bytes |

BSS increases are 2960 and 3680 bytes respectively. Color includes fixed
auxiliary tool-name storage. These are whole-image changes, including removal
of obsolete radio descriptors and conversions; color BSS includes existing
large display allocations.

## Validation and dependency audit

The single-parse loader is additionally checked by transactional tests for
uncommitted candidate state, malformed tails, duplicate keys, read failure,
calibration key ordering, inactive calibration-container preservation, source
and flex resolution, color tool-name isolation, and serial-mode conflicts.
Pocket firmware and monochrome simulator builds pass, along with monochrome
and color config/radio/model-storage/emergency tests: 82 monochrome tests passed
(one serial test skipped because fewer than two ports exist), and 86 color tests
passed. Filter: `RadioSettings.*:Config*.*:Yaml.*:Emergency*.*:Model.*:PartialModel.*`.

The following validation history describes the original migration:

Final builds passed for Pocket firmware, TX16S MK3 firmware, native simulator,
and monochrome/color native test binaries. The user also reported successful
builds of all 42 targets before the final verification changes; the complete
matrix was not rerun afterward.

- Monochrome: 105 selected tests passed.
- Color: 106 selected tests passed.
- Standalone core: all 18 tests passed with AddressSanitizer and
  UndefinedBehaviorSanitizer.

The regression filter was
`RadioSettings.*:ConfigFixture.*:Model.*:Sources.*:Yaml.*:EmergencySnapshot.*:MixerTest.*:Timers.*`.
Coverage includes defaults, repeated roundtrip, merge insertion, unknown scalar,
nested map/list and deep content, capability preservation, invalid values,
malformed input, read/short-write failures, temp-open failure, failed commit and
rollback, interrupted-commit recovery, debounce/forced flush, legacy aliases,
future version preservation, source inversion, CRSF enums, and direct lookup
of every bound field. Both capability sets load the shared compatibility
fixture. Bounds have assertions and overflow tests. Simulator FatFs fault hooks
exercise write and rename failures.

Searches find no generated-layout dependencies in the new radio configuration
files and no RadioData roots in the remaining generated YAML files. Radio-only
annotations, size checks, descriptor roots and conversion helpers were removed.
Model descriptors, `YamlNode`, `YamlTreeWalker`, `yaml_bits`, generator selection
and model size checks remain because model persistence still uses them. No RTC
or emergency-recovery source files changed. `git diff --check` passes.

## Deferred runtime simplifications

Explicit persistence bindings expose opportunities to unpack radio bitfields,
replace calibration unions with typed runtime state, consolidate capability
queries, and separate user control configuration from target-dependent runtime
storage. These are deliberately deferred. Model migration, arbitrary YAML
support, explicit unknown-field cleanup/backups and hardware stack/power-loss
validation remain separate work.
