# Labels storage

Labels use the streaming configuration engine. See also
[the completed reflection removal](generated-yaml-removal.md).

`ModelsList::loadYaml()` reads the existing `Labels`, `Sort`, and `Models`
mappings through `config_stream::process`. The dynamic mapping visitor in
`storage/labels_config.inc` uses the shared, fixed-size configuration workspace;
it retains no YAML tree or per-field schema. The ordinary model/label collections
remain the output, so parsing is not limited to `MaxFields` labels.

The first pass validates syntax and scalar bounds without changing collections.
The second pass applies the cache. A failed read discards any partial application,
rebuilds model cells from model headers, and blocks writes to `labels.yml` until a
successful reload. Startup remains usable and the rejected file stays available
for repair. The writer and model YAML loading are unchanged.

Compatibility includes case-insensitive section/attribute names, label order,
presence-based `selected` semantics (including `selected: false`), sort order,
file-existence checks, duplicate-model suppression, current-model selection,
hash-gated name/bitmap/labels/RF metadata, and unconditional `lastopen` loading.
Unknown sections are ignored. Oversized keys/scalars and invalid numeric fields
are rejected rather than copied or silently truncated into cache buffers.

## Storage dependencies

Radio, model, model-header, labels, and theme persistence use semantic adapters
and the streaming engine. The generated reflection infrastructure and its
CMake targets have been removed. Compatibility tests read checked-in legacy
YAML fixtures without invoking a generator.

`storage/sdcard_yaml.cpp/.h` remains in use for ordinary model/file operations
and wrappers around `model_config`.

## Initial migration validation

- TX16S MK3 (`STORAGE_MODELSLIST`) firmware build: passed.
- TX16S MK3 native `gtests-radio` and `simu` builds: passed.
- 57 tests passed across `ConfigFixture`, `ConfigInteger`, `ModelConfig`,
  `ModelConfigFile`, `PartialModel`, `ModelsList`, and `ModelMapFsTest`.
- Headless SDL simulator boot with an empty temporary SD directory, orderly
  timed shutdown, and restart from the saved files: passed. The intentionally
  empty SD directory has no splash image or widgets.
- `git diff --check`: passed.
