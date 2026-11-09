# This file is not intended to be executed - it's intended to be used to ensure type checker
# compatibility for names re-exported from the top-level `pedalboard` package.
# See: https://github.com/spotify/pedalboard/issues/333
#
# assert_type (rather than annotations like `x: T = expr`) fails if a type degrades to Any.

from typing import Mapping

import numpy as np
from numpy.typing import NDArray
from typing_extensions import assert_type

import pedalboard
from pedalboard import (
    AudioProcessorParameter,
    Chain,
    ExternalPlugin,
    Gain,
    Mix,
    Pedalboard,
    Plugin,
    Reverb,
    __version__,
    load_plugin,
    time_stretch,
)
from pedalboard.io import AudioFile, ReadableAudioFile, WriteableAudioFile

gain: pedalboard.Plugin
gain = pedalboard.Gain()
assert_type(pedalboard.Gain(), pedalboard.Gain)

plugin: Plugin = Gain(gain_db=3)
assert_type(Chain([Gain()]), Chain)
assert_type(Chain(), Chain)
assert_type(Mix([Gain()]), Mix)
assert_type(Mix(), Mix)
board = Pedalboard([Reverb(room_size=0.5), plugin, Chain([Gain()]), Mix([Gain()])])
assert_type(board, Pedalboard)

audio = np.zeros((2, 44100), dtype=np.float32)
assert_type(board(audio, 44100), NDArray[np.float32])
assert_type(time_stretch(audio, 44100, stretch_factor=1.5), NDArray[np.float32])

version: str = __version__  # Pyright infers a Literal, so assert_type can't be used here
external = load_plugin("some_plugin.vst3")
assert_type(external, ExternalPlugin)
assert_type(external.parameters, Mapping[str, AudioProcessorParameter])

assert_type(AudioFile("some_path"), ReadableAudioFile)
assert_type(AudioFile("some_path", "w", 44100, 2), WriteableAudioFile)

with AudioFile("some_path") as r:
    assert_type(r, ReadableAudioFile)
    r.read(r.frames)

with AudioFile("some_path", "w", 44100, 2) as w:
    assert_type(w, WriteableAudioFile)
    w.write(audio)
