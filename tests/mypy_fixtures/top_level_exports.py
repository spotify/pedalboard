# This file is not intended to be executed - it's intended to be used to ensure type checker
# compatibility for names re-exported from the top-level `pedalboard` package.
# See: https://github.com/spotify/pedalboard/issues/333

import numpy as np
from numpy.typing import NDArray

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

plugin: Plugin = Gain(gain_db=3)
board: Pedalboard = Pedalboard([Reverb(room_size=0.5), plugin, Chain([Gain()]), Mix([Gain()])])
audio = np.zeros((2, 44100), dtype=np.float32)
output: NDArray[np.float32] = board(audio, 44100)
stretched: NDArray[np.float32] = time_stretch(audio, 44100, stretch_factor=1.5)

version: str = __version__
external: ExternalPlugin = load_plugin("some_plugin.vst3")
parameter: AudioProcessorParameter = external.parameters["gain"]

with AudioFile("some_path") as r:
    readable: ReadableAudioFile = r
    readable.read(readable.frames)

with AudioFile("some_path", "w", 44100, 2) as w:
    writeable: WriteableAudioFile = w
    writeable.write(output)
