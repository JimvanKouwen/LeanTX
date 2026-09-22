#!/usr/bin/env python3
"""Keep audio engine types and state out of application callers."""
import re
from pathlib import Path

SRC = Path(__file__).resolve().parents[1] / "radio" / "src"
# Drivers consume the engine's buffers; DEBUG_AUDIO deliberately inspects it.
INTERNAL_USERS = {
    "audio.cpp",
    "audio_private.h",
    "cli.cpp",
    "boards/rm-h750/audio_driver.cpp",
    "targets/simu/audio_driver.cpp",
    "targets/stm32h7s78-dk/audio_driver.cpp",
    "targets/common/arm/stm32/audio_dac_driver.cpp",
    "targets/common/arm/stm32/vs1053b.cpp",
}
INTERNAL = re.compile(
    r'\baudio_private\.h\b|\b(?:audioQueue|audioBuffers|AudioQueue|AudioBuffer|'
    r'AudioBufferFifo|AudioFragment|AudioFragmentFifo|ToneContext|WavContext|'
    r'MixedContext|FragmentTypes|audio_data_t|AUDIO_QUEUE_LENGTH|'
    r'AUDIO_BUFFER_(?:SIZE|COUNT|DURATION))\b'
)


def check():
    failures = []
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in {".h", ".cpp", ".c", ".hpp"}:
            continue
        relative = path.relative_to(SRC).as_posix()
        if relative in INTERNAL_USERS or "thirdparty" in path.parts:
            continue
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if INTERNAL.search(line):
                failures.append(f"{relative}:{number}: {line.strip()}")
    return failures


if __name__ == "__main__":
    failures = check()
    if failures:
        print("Audio implementation leaked into callers:\n" + "\n".join(failures))
        raise SystemExit(1)
    print("Audio boundary check passed")
