# Legacy writer fixtures

Captured **before removal** with the retired EdgeTX generated YAML writer
from the existing migration tests, at repository base revision
dfaa35c51842b3b051458a10d673b5e1678edbd2
with the preceding streaming-labels migration applied.

- `full-model-color.yml`: TX16S MK3 native build.
- `full-model-mono.yml`: X9D+ 2019 native build.
- `curve-N-T.yml`: all 2..17 point counts, both curve types, smooth enabled,
  curve 31 named CNT. These are the unmodified `curves` mapping excerpts from
  the TX16S MK3 writer output; unrelated default model fields were omitted.

The full-model input values remain explicit in `LegacyFullModelRoundtrip`;
all original array equality and save/load stability assertions are retained.
`CurvePointCountEncodingIsUnchanged` retains every count/type combination and
checks reload after a streaming save, plus the smooth flag.

Both old-writer capture test runs passed. Fixtures retain their original CRLF
and indentation. Do not regenerate them with the new writer: they are the
independent compatibility oracle for the old wire representation.
