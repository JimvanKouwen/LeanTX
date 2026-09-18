#!/usr/bin/env python3
"""Guard RF implementation ownership and the sole production RC input path."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1] / "radio/src"
errors = []
private = re.compile(r'\b(moduleState|crossfireModuleStatus|pulsesGetModuleDriver|pulsesGetModuleBuffer|getModuleSyncStatus|setupPulsesCrossfire)\b|CrsfDevice::take|["<](?:pulses/)?rf_internal.h|["<]pulses/crossfire.h')
for path in root.rglob('*'):
    if path.suffix not in ('.cpp', '.h') or path.relative_to(root).parts[0] in ('pulses', 'tests', 'thirdparty'):
        continue
    for line, text in enumerate(path.read_text().splitlines(), 1):
        if private.search(text):
            errors.append(f'{path.relative_to(root)}:{line}: RF internals exposed')
service = (root / 'pulses/pulses.cpp').read_text()
assert 'const int16_t* channels = &channelOutputs[channelStart];' in service
assert 'MAX_OUTPUT_CHANNELS - CROSSFIRE_CHANNELS_COUNT' in service
for path in (root / 'pulses').glob('*.cpp'):
    for line, text in enumerate(path.read_text().splitlines(), 1):
        if 'channelOutputs' in text and text.strip() != 'const int16_t* channels = &channelOutputs[channelStart];':
            errors.append(f'{path.name}:{line}: unexpected RC source access')
if errors:
    raise SystemExit('\n'.join(errors))
print('RF boundary checks passed')
