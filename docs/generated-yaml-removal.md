# Generated YAML removal

Persistent YAML now uses semantic field adapters and the bounded streaming
engine for radio settings, full models, headers, labels, and themes. No YAML
path uses generated struct reflection, C++ byte/bit offsets, packed struct
sizes, or a raw runtime object image. Runtime model/radio packing has not been
broadly changed.

## Compatibility coverage

Before removing the writer, its migration tests captured full-model fixtures
on TX16S MK3 and X9D+ 2019, plus all 32 combinations of curve point counts 2–17
and types 0/1. The capture runs passed. The checked-in fixtures are independent
of the replacement writer and preserve the old whitespace/CRLF representation.
See [fixture provenance](../radio/src/tests/fixtures/legacy/README.md).

`LegacyFullModelRoundtrip` retains its array equality and stable save/load/save
assertions. `CurvePointCountEncodingIsUnchanged` retains every case and checks
the smooth flag as well. Other model behavior tests now exercise production
semantic loading/saving. The obsolete node-size/offset tests were replaced by
header/full-model semantic comparisons; a matching memory offset is no longer
a storage contract. Labels filesystem tests now write fixtures with the
production adapter and retain their active-screen/topbar isolation assertions.

Theme I/O was the last production caller of the old walker. It now has explicit
summary/color fields, accepts legacy RGB and hexadecimal colors, retains the
old document marker, and commits loaded values only after validation. Saves
use a temporary file and recoverable rename transaction. Tests cover text
escaping, missing-color defaults, invalid values, write failures and rename
failures. The prior writer also regenerated theme files rather than merging
unknown extensions; this behavior is unchanged.

The old parser tests also exposed that a bare empty RF type must mean RF off.
Both semantic model and header loading retain that legacy behavior.

## Removal audit

Removed 22 per-target generated tables: **10,557 generated C++ lines**. Their
shared reflection/parser/bit-conversion implementation, templates, helper
scripts, annotations, generated hardware YAML lookup, CMake targets and codegen
workflow/Justfile entries are also removed. In total, 44 legacy files containing
15,266 lines were deleted, including the preceding labels/model-list adapter
removal.

Hardware generation for ADCs, switches, keys and Lua remains: it has runtime
callers and does not generate persistent-storage object layouts. General AST
inspection tools are not persistence code and were retained. RTC backup
capacity/integrity assertions also remain; they protect a separate, versioned,
build-bound emergency snapshot format populated through explicit field copies.

The calibration adapter's old `CalibData`/`StepsCalibData` overlay cast was
replaced with a named union member. Runtime callers select that same member;
this preserves its storage size and removes the last calibration layout cast
from YAML adapters.

The labels cache file-info hash now explicitly encodes four size bytes, two
date bytes and two time bytes in the legacy little-endian order. A known-value
regression fixes the 16-character token independently of `FILINFO` layout.

## Next cleanup candidates

With the file format decoupled, packing and bitfields in `ModelData`,
`RadioData`, `ModelHeader`, mixers, inputs, timers, limits, curve headers,
telemetry sensors and RF module settings can be reviewed independently of YAML.
Unused spare/reserved fields are also candidates. These changes still need
RAM-budget, alignment, runtime copying and emergency-recovery review.

Board/LCD conditionals controlling string capacities, telemetry-screen
representations, script/sensor counts, function switches, or widgets can be
simplified without regenerating a storage schema. They still describe hardware
capabilities and memory budgets, so removing them indiscriminately is unsafe.
Keep protocol/DMA/USB packet layout requirements separate. Runtime calibration
checksums and any raw-copy code must be reviewed if its in-memory layout changes.

## Removal history

Commit `1ff24bd96` records the deleted files and annotation cleanup. Use
`git show --diff-filter=D --stat 1ff24bd96` for the complete deletion inventory.
This includes the old parser, walker, bit-conversion helpers, reflection tables,
layout assertions, generator scripts/templates, and their build definitions.

## Validation and measured impact

The pre-removal and final builds used the same Release configuration and ARM
toolchain, including the preceding labels migration. All five representative
firmware targets passed. Flash is ELF `text + data`; static RAM is `data + bss`
(including external SDRAM on TX16S MK3), not peak stack/heap usage.

| Target | Flash before | Flash after | Delta | Static RAM before | Static RAM after | Delta |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| x9dp2019 | 435,144 | 435,144 | +0 | 70,112 | 70,112 | +0 |
| pocket | 441,424 | 441,424 | +0 | 67,804 | 67,804 | +0 |
| tprov2 | 438,332 | 438,332 | +0 | 66,284 | 66,284 | +0 |
| commando8 | 414,772 | 414,772 | +0 | 58,876 | 58,876 | +0 |
| tx16smk3 | 1,373,824 | 1,369,184 | -4,640 | 11,446,452 | 11,446,436 | -16 |

Monochrome builds had already discarded unused reflection code at link time.
TX16S MK3 still used it for themes, so it benefits from removal. The runtime
`g_model` and `g_eeGeneral` object sizes remain unchanged (5,455 and 454 bytes
on TX16S MK3). Theme I/O reuses the bounded configuration workspace.

- TX16S MK3 native tests: **120 passed**, including all three theme tests.
- X9D+ 2019 native tests: **101 passed, one hardware-dependent skip**
  (`RadioSettings.SerialModesValidateAgainstCandidateNotLivePorts`).
- Suites cover radio settings, streaming syntax, model configuration/file
  transactions, model/header compatibility, labels and model-list startup,
  RF safety, theme I/O, source names and emergency snapshots.
- Both native simulator builds passed; both booted and shut down cleanly from
  empty temporary storage and restarted from the saved files under headless SDL.
  The color smoke test also discovered and loaded a legacy RGB/hex theme.
- Source/reference audit and all five firmware symbol tables contain none of
  the retired parser/walker/model-reflection entry points.
- `just --list` and `git diff --check` passed.

Persistent YAML has **zero remaining dependency on generated struct reflection
or exact runtime memory layout**. The separate RTC emergency snapshot keeps
its own versioned, build-specific binary representation; it does not serialize
`ModelData` or `RadioData` as raw object images and is not a YAML format.
