# Legacy writer fixtures

Captured **before removal** with the retired EdgeTX generated YAML writer
from the existing migration tests, at repository base revision
dfaa35c51842b3b051458a10d673b5e1678edbd2
with the preceding streaming-labels migration applied.

- `full-model-color.yml`: TX16S MK3 native build.
- `full-model-mono.yml`: X9D+ 2019 native build.

The full-model input values remain explicit in `LegacyFullModelRoundtrip`.
Array equality and save/load stability assertions cover the retained fields.

Both old-writer capture test runs passed. Removed subsystem fields have been
stripped. Fixtures retain their original CRLF and indentation. Do not regenerate
them with the new writer: they are the independent compatibility oracle for the
old wire representation.
