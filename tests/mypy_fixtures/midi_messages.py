# This file is not intended to be executed - it's intended to be used to ensure type checker
# compatibility when passing MIDI messages to external instrument plugins.

from typing import List, Tuple

import numpy as np
from mido import Message  # type: ignore[import-untyped]  # mido ships no type hints
from numpy.typing import NDArray
from typing_extensions import assert_type

from pedalboard import load_plugin

instrument = load_plugin("some_instrument.vst3")


class ReadOnlyMessage:
    @property
    def time(self) -> float:
        return 0.0

    def bytes(self) -> List[int]:
        return [0x90, 60, 100]


# mido messages:
mido_output = instrument(
    [Message("note_on", note=60), Message("note_off", note=60, time=4)],
    duration=5,
    sample_rate=44100,
)
assert_type(mido_output, NDArray[np.float32])

# Lists stored in variables, rather than passed as literals:
byte_messages: List[Tuple[bytes, float]] = [(b"\x90\x3c\x64", 0.0)]
instrument(byte_messages, duration=1.0, sample_rate=44100)
int_messages: List[Tuple[List[int], float]] = [([0x90, 60, 100], 0.0)]
instrument.process(int_messages, duration=1.0, sample_rate=44100, num_channels=2)

# Any object with a (read-only) `time` and a `bytes()` method:
instrument([ReadOnlyMessage()], duration=1.0, sample_rate=44100)
